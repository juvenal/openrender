/**
 * tests/shading_contract/case_string_message_far.cpp
 *
 * ShadingContract_StringMessageFarPointer (GitHub #176). The interpreter's surface(), displacement() and atmosphere() copy a
 * producer's string parameter from the producer's locals into the caller's destination. The copy used to move one float
 * (PARAMETEREXPRS), the low 32 bits of the pointer, and kept the destination's high 32 bits: a pointer whose high half differs
 * from the destination's previous value came out mangled (an intermittent SIGBUS in strcmp). The test puts a string at an
 * address whose high 32 bits differ from the destination's, runs a shader that queries it, and compares the pointer that
 * arrives by value. It never dereferences the result, so a failure is an assertion and not a crash.
 *
 * lightsource() is covered too, in an illuminance loop over a light with an output string parameter: it reads the light's saved
 * state, which saveLighting (execute.cpp) used to fill with sizeof(float) per string, half a pointer. The test puts the far
 * pointer into the light instance's default, so the light's locals, its saved state and the caller's destination all carry it.
 * Only the interpreter is covered here (the JIT keeps a string output parameter in its saved state since GitHub #197 and reads it
 * through op_lightsource_param since GitHub #115; ShadingContract_LightsourceRispecJit_Types reads a string parameter).
 */
#include <cstdint>
#include <cstring>
#include <sys/mman.h>

#include "case_common.h"
#include "rendered_cases.h"
#include "rendered_harness.h"
#include "ri/state/stateHandles.h"

using namespace openrender::shading;

namespace {

    CVariable *findParameter(CProgrammableShaderInstance *instance, const char *name) {
        for (CVariable *v = instance->parameters; v != nullptr; v = v->next)
            if (strcmp(v->name, name) == 0)
                return v;
        return nullptr;
    }

} // namespace

int caseStringMessageFar() {
    CRenderedSession s(false); // the interpreter only: the JIT copies a whole char *
    CRProbeContext *ctx = s.makeContext(s.original);

    CProgrammableShaderInstance *pSurface = s.load("cm_producer_surface", SL_SURFACE);
    CProgrammableShaderInstance *pDisplacement = s.load("cm_producer_displacement", SL_DISPLACEMENT);
    CProgrammableShaderInstance *pAtmosphere = s.load("cm_producer_volume", SL_ATMOSPHERE);
    CProgrammableShaderInstance *consumer = s.load("cm_str_far", SL_SURFACE);
    CT_EXPECT(pSurface && pDisplacement && pAtmosphere && consumer);
    if (!(pSurface && pDisplacement && pAtmosphere && consumer)) {
        delete ctx;
        return contract_test::finish();
    }
    s.attributes->bindSurface(ShaderGroupRef::retain(pSurface));
    s.attributes->bindDisplacement(ShaderGroupRef::retain(pDisplacement));
    s.attributes->bindAtmosphere(ShaderGroupRef::retain(pAtmosphere));

    CRenderedSession::setupState(ctx, 3, s.object);
    CShadingState *state = ctx->currentShadingState;
    state->currentShaderInstance = consumer;

    CMemPage *producerMemory = NULL;
    memoryInit(producerMemory);
    state->locals[ACCESSOR_SURFACE] = pSurface->prepare(producerMemory, state->varying, 3);
    state->locals[ACCESSOR_DISPLACEMENT] = pDisplacement->prepare(producerMemory, state->varying, 3);
    state->locals[ACCESSOR_ATMOSPHERE] = pAtmosphere->prepare(producerMemory, state->varying, 3);

    CMemPage *localMemory = NULL;
    memoryInit(localMemory);
    float **locals = consumer->prepare(localMemory, state->varying, state->numVertices);

    struct SRow {
            const char *param;
            CProgrammableShaderInstance *producer;
            int accessor;
    };
    const SRow rows[] = {{"s1", pSurface, ACCESSOR_SURFACE}, {"s2", pDisplacement, ACCESSOR_DISPLACEMENT}, {"s3", pAtmosphere, ACCESSOR_ATMOSPHERE}};

    // A page whose address differs from the destination's initial string ("unset", a strdup'd constant) in the high 32 bits
    CVariable *first = findParameter(consumer, rows[0].param);
    CT_EXPECT(first != nullptr);
    if (first == nullptr) {
        delete ctx;
        return contract_test::finish();
    }
    const uintptr_t unsetPtr = (uintptr_t) * (char **)locals[first->entry];
    void *hint = (void *)((unsetPtr + ((uintptr_t)8 << 30)) & ~(uintptr_t)0xFFFF);
    const size_t pageSize = 1 << 16;
    void *page = mmap(hint, pageSize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    const bool haveFar = (page != MAP_FAILED) && (((uintptr_t)page >> 32) != (unsetPtr >> 32));
    if (!haveFar)
        fprintf(stderr, "string_message_far: cannot place a string with different high 32 bits than %p (mmap hint %p gave %p)\n", (void *)unsetPtr, hint,
                page == MAP_FAILED ? (void *)0 : page);
    CT_EXPECT(haveFar); // never a silent skip: without a far address the test proves nothing
    if (!haveFar) {
        if (page != MAP_FAILED)
            munmap(page, pageSize);
        delete ctx;
        return contract_test::finish();
    }
    char *farString = (char *)page + 0x1238; // an offset, so the low 32 bits are not zero either
    strcpy(farString, "far-away");

    // Replace each producer's current string value with the far one: this is what the caller reads through the variable
    for (const SRow &row : rows) {
        CVariable *pv = findParameter(row.producer, "cmstring");
        CT_EXPECT(pv != nullptr);
        if (pv != nullptr)
            *(char **)state->locals[row.accessor][pv->entry] = farString;
    }

    ctx->execute(consumer, locals);

    for (const SRow &row : rows) {
        CVariable *cv = findParameter(consumer, row.param);
        CT_EXPECT(cv != nullptr);
        if (cv == nullptr)
            continue;
        const uintptr_t got = (uintptr_t) * (char **)locals[cv->entry];
        printf("  %s: got %p, want %p\n", row.param, (void *)got, (void *)farString);
        CT_EXPECT(got == (uintptr_t)farString); // compared by value, never dereferenced
    }

    // lightsource(): a light with an output string parameter, run by the consumer's illuminance loop
    {
        CProgrammableShaderInstance *light = s.load("cm_producer_light_str", SL_LIGHTSOURCE);
        CProgrammableShaderInstance *lsConsumer = s.load("cm_lightsource_str", SL_SURFACE);
        CT_EXPECT(light && lsConsumer);
        CVariable *lv = light ? findParameter(light, "cmstring") : nullptr;
        CT_EXPECT(lv != nullptr);
        if (light && lsConsumer && lv) {
            *(char **)lv->defaultValue = farString;
            s.attributes->addLight(light);

            CRenderedSession::setupState(ctx, 3, s.object);
            state->currentShaderInstance = lsConsumer;
            CMemPage *lsMemory = NULL;
            memoryInit(lsMemory);
            float **lsLocals = lsConsumer->prepare(lsMemory, state->varying, state->numVertices);
            CVariable *sv = findParameter(lsConsumer, "s4");
            CT_EXPECT(sv != nullptr);
            ctx->execute(lsConsumer, lsLocals);
            if (sv != nullptr) {
                const uintptr_t got = (uintptr_t) * (char **)lsLocals[sv->entry];
                printf("  s4 (lightsource): got %p, want %p\n", (void *)got, (void *)farString);
                CT_EXPECT(got == (uintptr_t)farString);
            }
            state->lights = nullptr;
            state->freeLights = nullptr;
            state->currentShaderInstance = nullptr;
            memoryTini(lsMemory);
        }
    }

    state->currentShaderInstance = nullptr;
    memoryTini(localMemory);
    munmap(page, pageSize);
    delete ctx;
    return contract_test::finish();
}

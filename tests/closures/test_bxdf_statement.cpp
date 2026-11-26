/*
 * Spec 035, Phase 4 (T083), labels closures, bxdf: the Bxdf statement as state (contracts/bxdf.md "RiBxdf / RiBxdfV"). One program, one section
 * per ctest (argv[1]).
 *
 *   LastBound     Bxdf_LastBound: Bxdf then Surface in one block: Surface governs; swapped, Bxdf governs; after AttributeEnd the enclosing
 *                 choice returns (US4 AS1, D5)
 *   ColorSamples  Bxdf_ColorSamples: a color parameter under ColorSamples with n != 3 is converted to RGB as RiColor's are, through
 *                 COptions::convertColor (RISpec 4.1.3); read back through the built tree
 *   ObjectBlock   Bxdf_ObjectBlock: Bxdf inside ObjectBegin/ObjectEnd is refused by check() through VALID_BXDF_BLOCKS (Q-R4); Surface and
 *                 Attribute there behave as today
 *   Refusals      Bxdf_Refusals: an unknown name, "sl:"/"osl:" and a refused MAJOR each give a diagnostic naming the reason and the name; the
 *                 binding is unchanged; no instance key is pending and shaderOrdinal has not advanced (US4 AS3, FR-016: observed as a later
 *                 random()-drawing Surface that renders byte for byte as the control)
 *   Parameters    Bxdf_Parameters: Ka is an unknown parameter (CODE_BADTOKEN, skipped), other out-of-range values are clamped (CODE_RANGE naming
 *                 the parameter), a negative plastic ior and a dielectric ior of 0 refuse the statement (T082)
 *   NoKey         Bxdf_NoKey: a scene with Bxdf statements has the same instance keys and draws for every later shader as the scene without
 *   Instancing    Bxdf_Instancing: Bxdf A in the scope that encloses ObjectBegin, Bxdf B before ObjectInstance: B governs the instance's
 *                 primitives and A does not (C3, Q-R2, RISpec 5.10)
 *   RibRoundTrip  Bxdf_RibRoundTrip: statement, handle, parameters and place relative to Surface survive parse, ribOut and parse; the grammar
 *                 with and without a handle, and a bare-string first parameter in brackets
 *   Network       Bxdf_Network: RiBxdfV of a -P:N client returns early (R12)
 *
 * Red on the starting tree: does not compile (bxdfInstance.h, RiBxdf, VALID_BXDF_BLOCKS).
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "bxdf_support.h"
#include "bxdf_tree_support.h"
#include "bxdfInstance.h"
#include "bxdfRegistry.h"
#include "attributes.h"
#include "options.h"
#include "renderer.h"
#include "rendererContext.h"
#include "riInterface.h"
#include "rib.h"

using namespace clbx;
using namespace bxtree;
using openrender::shading::SBxdfLookup;

namespace {

    struct Diagnostic {
            int code, severity;
            std::string message;
    };
    std::vector<Diagnostic> g_diagnostics;
    void capture(RtInt code, RtInt severity, const char *message) { g_diagnostics.push_back({code, severity, message}); }
    bool anyDiagnosticNaming(const std::string &a, const std::string &b = "") {
        for (const Diagnostic &d : g_diagnostics)
            if (d.message.find(a) != std::string::npos && (b.empty() || d.message.find(b) != std::string::npos))
                return true;
        return false;
    }
    int diagnosticsWithCode(int code) {
        int n = 0;
        for (const Diagnostic &d : g_diagnostics)
            n += d.code == code ? 1 : 0;
        return n;
    }

    CAttributes *attributes() { return CRenderer::context->getAttributes(FALSE); }
    // The Bxdf instance the current attributes carry, or nullptr (bxdfInstanceOf: a foreign object in the slot answers nullptr)
    openrender::shading::CBxdfInstance *current() { return openrender::shading::bxdfInstanceOf(attributes()->bxdf()); }
    std::string currentName() { return current() != nullptr ? current()->table->name : "<none>"; }

    // The tree of the current instance at the built-in tests' point
    std::vector<Lobe> currentLobes(bool &ok) {
        ok = false;
        if (current() == nullptr)
            return {};
        return lobesOfData(current()->table, current()->data, pointWith({1, 1, 1}), ok);
    }

    // ---- LastBound ---------------------------------------------------------------------------------------------------------
    void sectionLastBound() {
        RiBegin(RI_NULL);
        RiErrorHandler(capture);
        RiWorldBegin();
        CL_CHECK(attributes()->governingMaterial() == EMaterialBinding::None);

        RiAttributeBegin();
        bxdf("diffuse");
        RiSurface("null", RI_NULL);
        CL_CHECK(attributes()->governingMaterial() == EMaterialBinding::Surface); // Bxdf then Surface: Surface governs
        CL_CHECK(current() != nullptr);                                           // the Bxdf is still bound, it just does not govern
        RiSurface("null", RI_NULL);
        bxdf("conductor");
        CL_CHECK(attributes()->governingMaterial() == EMaterialBinding::Bxdf); // swapped: Bxdf governs
        CL_CHECK(currentName() == "conductor");
        RiAttributeEnd();
        CL_CHECK(attributes()->governingMaterial() == EMaterialBinding::None); // the enclosing choice returns
        CL_CHECK(current() == nullptr);

        // an enclosing Surface returns after a nested Bxdf block, and the enclosing Bxdf after a nested Surface block
        RiSurface("null", RI_NULL);
        RiAttributeBegin();
        bxdf("plastic");
        CL_CHECK(attributes()->governingMaterial() == EMaterialBinding::Bxdf);
        RiAttributeEnd();
        CL_CHECK(attributes()->governingMaterial() == EMaterialBinding::Surface);
        bxdf("dielectric");
        RiAttributeBegin();
        RiSurface("null", RI_NULL);
        CL_CHECK(attributes()->governingMaterial() == EMaterialBinding::Surface);
        RiAttributeEnd();
        CL_CHECK(attributes()->governingMaterial() == EMaterialBinding::Bxdf);
        CL_CHECK(currentName() == "dielectric");
        RiWorldEnd();
        RiEnd();
        CL_CHECK(g_diagnostics.empty());
    }

    // ---- ColorSamples ------------------------------------------------------------------------------------------------------
    // ColorSamples 2 (an option, before the world): nRGB is the 2 x 3 matrix that takes a 2-component colour to RGB. With
    // nRGB = (1 0.5 0.5 / 0.5 0.5 0) the colour (0.4, 0.2) is the RGB (0.5, 0.3, 0.2): plastic's Ks * specularcolor = 0.5 * (0.5, 0.3, 0.2).
    void sectionColorSamples() {
        RiBegin(RI_NULL);
        RiErrorHandler(capture);
        // A matrix that reads the same as n x 3 (RISpec 3.2) and as the transposed 3 x n that openRender reads today (GitHub #269)
        RtFloat nRGB[6] = {1, 0.5f, 0.5f, 0.5f, 0.5f, 0};
        RtFloat RGBn[6] = {1, 0, 0, -1, 1, 0};
        RiColorSamples(2, nRGB, RGBn);
        RiWorldBegin();
        RtFloat sc[2] = {0.4f, 0.2f};
        RiBxdf("plastic", RI_NULL, "color specularcolor", sc, RI_NULL);
        CL_CHECK(g_diagnostics.empty());
        CL_CHECK(current() != nullptr);
        bool ok;
        const std::vector<Lobe> got = currentLobes(ok);
        const std::vector<Lobe> want = {diffuse(times(0.5f, kCs)), ggx(times(0.5f, {0.5f, 0.3f, 0.2f}), 0.1f, 0.0f, 0)};
        CL_CHECK(ok && nearLobes(got, want, 1e-6f));
        RiWorldEnd();
        RiEnd();
    }

    // ---- ObjectBlock -------------------------------------------------------------------------------------------------------
    void sectionObjectBlock() {
        RiBegin(RI_NULL);
        RiErrorHandler(capture);
        RiWorldBegin();
        bxdf("diffuse");
        RtObjectHandle object = RiObjectBegin();
        g_diagnostics.clear();
        bxdf("plastic"); // refused as an out-of-block request: check() reports "Bad scope" and the statement is ignored
        CL_CHECK(diagnosticsWithCode(RIE_NESTING) == 1);
        CL_CHECK(anyDiagnosticNaming("Bad scope", "RiBxdf"));
        CL_CHECK(currentName() == "diffuse"); // the binding is unchanged
        g_diagnostics.clear();
        RiSurface("null", RI_NULL); // Surface and Attribute inside ObjectBegin behave as today (Q-R4)
        RtString format = const_cast<RtString>("rslo");
        RiAttribute(RI_SHADE, "shaderformat", &format, RI_NULL);
        CL_CHECK(g_diagnostics.empty());
        CL_CHECK(attributes()->governingMaterial() == EMaterialBinding::Surface);
        RiObjectEnd();
        (void)object;
        // outside the block the same statement is accepted
        bxdf("plastic");
        CL_CHECK(currentName() == "plastic");
        RiWorldEnd();
        RiEnd();
    }

    // ---- Refusals ----------------------------------------------------------------------------------------------------------
    // The control draws random() in a Surface; each variant puts one refused Bxdf statement (or none) before the Surface. If a refusal left a
    // key pending, or advanced the shader ordinal, the Surface would draw other numbers and the image would differ. The control is rendered
    // twice: the scene is deterministic, so a difference is a key and not noise.
    SRun randomScene(const std::function<void()> &statements, const char *hider = "raytrace") {
        SBxdfScene s;
        s.hider = hider;
        s.front = [&]() {
            if (statements)
                statements();
            RiSurface("cl_random", RI_NULL);
        };
        return render(s);
    }

    void sectionRefusals() {
        const SRun control = randomScene(nullptr);
        CL_CHECK(randomScene(nullptr).image == control.image);
        CL_CHECK(control.image.at(3, 3, 0) > 0.0f);

        for (const char *name : {"p4_no_such_bxdf", "sl:myshader", "osl:myshader"}) {
            const SRun run = randomScene([&]() { RiBxdf(name, RI_NULL, RI_NULL); });
            CL_CHECK(run.image == control.image); // no key leaked
            CL_CHECK(run.errors == 1);            // one CODE_BADTOKEN error naming the statement, the name and the reason
        }
        // and the binding stays what it was: a Bxdf bound first survives a refused statement
        SBxdfScene s;
        s.noFront = true;
        s.world = []() {
            bxdf("conductor");
            g_diagnostics.clear();
            RiErrorHandler(capture);
            RiBxdf("osl:nope", RI_NULL, RI_NULL);
            CL_CHECK(g_diagnostics.size() == 1);
            CL_CHECK(anyDiagnosticNaming("RiBxdf") && anyDiagnosticNaming("osl:nope") && anyDiagnosticNaming("RSL 2.0"));
            CL_CHECK(currentName() == "conductor");
            g_diagnostics.clear();
            RiBxdf("p4_missing", RI_NULL, RI_NULL);
            CL_CHECK(g_diagnostics.size() == 1 && anyDiagnosticNaming("p4_missing"));
            CL_CHECK(currentName() == "conductor");
        };
        render(s);
        // a refused MAJOR: registered with a MAJOR the host does not support, the statement names that reason
        static OrBxdfTable future;
        std::memset(&future, 0, sizeof(future));
        future.header.structSize = sizeof(future);
        future.header.abiVersion = OR_ABI_VERSION(2, 0, 0);
        future.name = "p4_future";
        openrender::shading::registerBxdf(&future, "test_bxdf_statement");
        const SRun run = randomScene([]() { RiBxdf("p4_future", RI_NULL, RI_NULL); });
        CL_CHECK(run.image == control.image && run.errors == 1);
        SBxdfScene named;
        named.noFront = true;
        named.world = []() {
            g_diagnostics.clear();
            RiErrorHandler(capture);
            RiBxdf("p4_future", RI_NULL, RI_NULL);
            CL_CHECK(g_diagnostics.size() == 1 && anyDiagnosticNaming("p4_future"));
            CL_CHECK(anyDiagnosticNaming("MAJOR") || anyDiagnosticNaming("major")); // the reason: the unsupported MAJOR
            CL_CHECK(current() == nullptr);
        };
        render(named);
    }

    // ---- Parameters --------------------------------------------------------------------------------------------------------
    void sectionParameters() {
        RiBegin(RI_NULL);
        RiErrorHandler(capture);
        RiWorldBegin();
        bool ok;
        // Ka is not a parameter: CODE_BADTOKEN naming it, the parameter skipped, the statement still binds with the defaults
        RtFloat ka = 0.7f;
        RiBxdf("diffuse", RI_NULL, "float Ka", &ka, RI_NULL);
        CL_CHECK(diagnosticsWithCode(RIE_BADTOKEN) == 1 && anyDiagnosticNaming("Ka"));
        CL_CHECK(currentName() == "diffuse");
        CL_CHECK(sameLobes(currentLobes(ok), {diffuse(times(1.0f, kCs))}) && ok);

        // clamps: Ks below 0 is 0 and roughness above 1 is 1; CODE_RANGE naming the parameter each time
        g_diagnostics.clear();
        RtFloat negative = -1.0f, big = 2.0f;
        RiBxdf("conductor", RI_NULL, "float Ks", &negative, "float roughness", &big, RI_NULL);
        CL_CHECK(diagnosticsWithCode(RIE_RANGE) == 2 && anyDiagnosticNaming("Ks") && anyDiagnosticNaming("roughness"));
        CL_CHECK(currentName() == "conductor");
        // Ks clamped to 0 makes the lobe's weight 0, and the flattened list holds no zero-weight lobe: the tree is non-null, the list is empty
        CL_CHECK(sameLobes(currentLobes(ok), {}) && ok);
        // roughness alone: 2 is clamped to 1, which is the alpha of the lobe
        g_diagnostics.clear();
        RiBxdf("conductor", RI_NULL, "float roughness", &big, RI_NULL);
        CL_CHECK(diagnosticsWithCode(RIE_RANGE) == 1 && anyDiagnosticNaming("roughness"));
        CL_CHECK(sameLobes(currentLobes(ok), {ggx(times(1.0f, kCs), 1.0f, 0.0f, 0)}) && ok);

        // refusals: a negative plastic ior and a dielectric ior of 0 and below refuse the statement; the previous binding stays
        g_diagnostics.clear();
        RtFloat ior = -1.0f;
        RiBxdf("plastic", RI_NULL, "float ior", &ior, RI_NULL);
        CL_CHECK(diagnosticsWithCode(RIE_RANGE) == 1 && anyDiagnosticNaming("ior"));
        CL_CHECK(currentName() == "conductor");
        g_diagnostics.clear();
        ior = 0.0f;
        RiBxdf("dielectric", RI_NULL, "float ior", &ior, RI_NULL);
        CL_CHECK(diagnosticsWithCode(RIE_RANGE) == 1 && anyDiagnosticNaming("ior"));
        CL_CHECK(currentName() == "conductor");
        ior = -2.0f;
        g_diagnostics.clear();
        RiBxdf("dielectric", RI_NULL, "float ior", &ior, RI_NULL);
        CL_CHECK(diagnosticsWithCode(RIE_RANGE) == 1 && currentName() == "conductor");
        // a valid ior binds
        g_diagnostics.clear();
        ior = 1.33f;
        RiBxdf("dielectric", RI_NULL, "float ior", &ior, RI_NULL);
        CL_CHECK(g_diagnostics.empty() && currentName() == "dielectric");
        RiWorldEnd();
        RiEnd();
    }

    // ---- NoKey -------------------------------------------------------------------------------------------------------------
    // Bxdf statements, successful ones included, take no instance key and advance no shader ordinal: the random()-drawing Surface that follows
    // renders as it does without them (FR-016). Three hiders.
    void sectionNoKey() {
        for (const char *hider : {"raytrace", "zbuffer", "reyes"}) {
            const SRun control = randomScene(nullptr, hider);
            CL_CHECK(control.image.at(3, 3, 0) > 0.0f);
            const SRun withBxdf = randomScene(
                [&]() {
                    bxdf("diffuse");
                    RtFloat kd = 0.25f;
                    RiBxdf("plastic", "h1", "float Kd", &kd, RI_NULL);
                    bxdf("conductor");
                },
                hider);
            CL_CHECK(withBxdf.image == control.image);
            CL_CHECK(withBxdf.errors == 0);
            // a Bxdf bound in the world, ahead of the object, and one before WorldBegin: the same
            SBxdfScene s;
            s.hider = hider;
            s.preWorld = []() { bxdf("dielectric"); };
            s.world = []() { bxdf("conductor"); };
            s.front = []() { RiSurface("cl_random", RI_NULL); };
            CL_CHECK(render(s).image == control.image);
        }
    }

    // ---- Instancing --------------------------------------------------------------------------------------------------------
    // Bxdf A is bound in the attribute scope that encloses ObjectBegin; B is bound before ObjectInstance. The instance's primitives take the
    // binding current at ObjectInstance (RISpec 5.10): B governs, A does not. Checked through the delivered trees of a consumer (a diffuse
    // tree has one lobe, a conductor's has a microfacet) and through the last-bound record.
    void sectionInstancing() {
        SBxdfScene s;
        s.consumer = true;
        s.noFront = true;
        s.world = []() {
            RtPoint quad[4] = {{-4, -4, 1}, {4, -4, 1}, {4, 4, 1}, {-4, 4, 1}};
            RtColor c = {0.25f, 0.5f, 0.75f};
            RiColor(c);
            RiAttributeBegin();
            bxdf("diffuse"); // A, in the scope that encloses ObjectBegin
            RtObjectHandle h = RiObjectBegin();
            RiPolygon(4, RI_P, (RtPointer)quad, RI_NULL);
            RiObjectEnd();
            bxdf("conductor"); // B, before ObjectInstance
            CL_CHECK(currentName() == "conductor");
            RiObjectInstance(h);
            RiAttributeEnd();
        };
        CConsumerDouble consumerRecord;
        const SRun run = render(s, &consumerRecord);
        CL_CHECK(run.trees > 0 && consumerRecord.fromBxdf() > 0);
        // the hand-built conductor trees at the delivered points: every delivered tree is one microfacet lobe, none is a diffuse one
        SBxdfScene reference = s;
        reference.world = [&]() {
            RtPoint quad[4] = {{-4, -4, 1}, {4, -4, 1}, {4, 4, 1}, {-4, 4, 1}};
            RtColor c = {0.25f, 0.5f, 0.75f};
            RiColor(c);
            RiAttributeBegin();
            bxdf("conductor");
            RiPolygon(4, RI_P, (RtPointer)quad, RI_NULL);
            RiAttributeEnd();
        };
        CConsumerDouble referenceRecord;
        const SRun ref = render(reference, &referenceRecord);
        CL_CHECK(ref.trees == run.trees && ref.hashes == run.hashes); // the instance is the conductor object, byte for byte
        SBxdfScene wrong = s;
        wrong.world = [&]() {
            RtPoint quad[4] = {{-4, -4, 1}, {4, -4, 1}, {4, 4, 1}, {-4, 4, 1}};
            RtColor c = {0.25f, 0.5f, 0.75f};
            RiColor(c);
            RiAttributeBegin();
            bxdf("diffuse");
            RiPolygon(4, RI_P, (RtPointer)quad, RI_NULL);
            RiAttributeEnd();
        };
        CConsumerDouble wrongRecord;
        const SRun other = render(wrong, &wrongRecord);
        CL_CHECK(other.hashes != run.hashes); // and not the diffuse one (A)
    }

    // ---- RibRoundTrip ------------------------------------------------------------------------------------------------------
    std::string slurp(const std::string &path) {
        std::ifstream f(path);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
    void writeFile(const std::string &path, const std::string &text) {
        std::ofstream f(path);
        f << text;
    }
    // What a RIB leaves bound after parsing: the name and handle of the Bxdf, the governing material
    struct Bound {
            std::string name, handle;
            EMaterialBinding governing;
            bool operator==(const Bound &o) const { return name == o.name && handle == o.handle && governing == o.governing; }
    };
    Bound parseWithRenderer(const std::string &rib) {
        RiBegin(RI_NULL);
        RiReadArchive((RtString)rib.c_str(), nullptr, RI_NULL);
        Bound b{current() != nullptr ? current()->table->name : "<none>", current() != nullptr ? current()->handle : "", attributes()->governingMaterial()};
        RiEnd();
        return b;
    }
    void parseIntoWriter(const std::string &rib, const std::string &out) {
        RiBegin((RtToken)out.c_str());
        CRenderer::initDeclarations();
        globalArena.init();
        ribParse(rib.c_str(), nullptr);
        globalArena.tini();
        CRenderer::shutdownDeclarations();
        RiEnd();
    }
    void sectionRibRoundTrip() {
        char tmpl[] = "/tmp/bxdf_roundtrip_XXXXXX";
        const std::string dir = mkdtemp(tmpl);
        struct Case {
                const char *label;
                const char *text;
                Bound expected;
        };
        // WorldBegin is not needed: a Bxdf is legal wherever an attribute is, and the archive reader is a frame-less block (VALID_BXDF_BLOCKS)
        const Case cases[] = {
            {"plain", "Bxdf \"diffuse\"\n", {"diffuse", "", EMaterialBinding::Bxdf}},
            {"handle", "Bxdf \"plastic\" \"h7\"\n", {"plastic", "h7", EMaterialBinding::Bxdf}},
            {"params", "Bxdf \"plastic\" \"float Kd\" [0.25] \"float roughness\" [0.3]\n", {"plastic", "", EMaterialBinding::Bxdf}},
            {"handle and params", "Bxdf \"conductor\" \"m1\" \"float Ks\" [0.5]\n", {"conductor", "m1", EMaterialBinding::Bxdf}},
            // a first parameter whose value is a bare string must be written in brackets when there is no handle (the LightSource precedent)
            {"bracketed", "Bxdf \"diffuse\" \"string note\" [\"x\"]\n", {"diffuse", "", EMaterialBinding::Bxdf}},
            // the place relative to Surface: the order decides which governs
            {"surface then bxdf", "Surface \"matte\"\nBxdf \"diffuse\"\n", {"diffuse", "", EMaterialBinding::Bxdf}},
            {"bxdf then surface", "Bxdf \"diffuse\"\nSurface \"matte\"\n", {"diffuse", "", EMaterialBinding::Surface}},
        };
        for (const Case &c : cases) {
            const std::string in = dir + "/in.rib", written = dir + "/written.rib", again = dir + "/again.rib";
            writeFile(in, c.text);
            CL_CHECK(parseWithRenderer(in) == c.expected);
            parseIntoWriter(in, written);
            const std::string text = slurp(written);
            const size_t bxdfAt = text.find("Bxdf \"");
            CL_CHECK(bxdfAt != std::string::npos);
            if (std::strstr(c.text, "Surface") != nullptr) {
                const size_t surfaceAt = text.find("Surface \"");
                CL_CHECK(surfaceAt != std::string::npos);
                CL_CHECK((std::strstr(c.text, "Surface \"matte\"\nBxdf") != nullptr) == (surfaceAt < bxdfAt)); // the place is kept
            }
            if (c.expected.handle != "")
                CL_CHECK(text.find("\"" + c.expected.handle + "\"") != std::string::npos);
            if (c.expected.handle == "")
                CL_CHECK(text.find("Bxdf \"" + c.expected.name + "\" \"\"") == std::string::npos); // no handle is written for a null one
            CL_CHECK(parseWithRenderer(written) == c.expected);
            parseIntoWriter(written, again);
            CL_CHECK(slurp(again) == text); // the writer's output is stable
            for (const std::string &p : {in, written, again})
                unlink(p.c_str());
        }
        rmdir(dir.c_str());
    }

    // ---- Network -----------------------------------------------------------------------------------------------------------
    // A -P:N client never shades: RiBxdfV returns first thing (R12), as RiSurfaceV does. No binding, no diagnostic, not even for a name the
    // registry would refuse.
    void sectionNetwork() {
        RiBegin(RI_NULL);
        RiErrorHandler(capture);
        RiWorldBegin();
        const int saved = CRenderer::netNumServers;
        CRenderer::netNumServers = 2;
        bxdf("diffuse");
        RiBxdf("p4_no_such_bxdf", RI_NULL, RI_NULL);
        CRenderer::netNumServers = saved;
        CL_CHECK(current() == nullptr);
        CL_CHECK(attributes()->governingMaterial() == EMaterialBinding::None);
        CL_CHECK(g_diagnostics.empty());
        bxdf("diffuse"); // and with no servers the same statement binds
        CL_CHECK(currentName() == "diffuse");
        RiWorldEnd();
        RiEnd();
    }

} // namespace

int main(int argc, char **argv) {
    cldbl::registerBackend();
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "LastBound")
        sectionLastBound();
    else if (which == "ColorSamples")
        sectionColorSamples();
    else if (which == "ObjectBlock")
        sectionObjectBlock();
    else if (which == "Refusals")
        sectionRefusals();
    else if (which == "Parameters")
        sectionParameters();
    else if (which == "NoKey")
        sectionNoKey();
    else if (which == "Instancing")
        sectionInstancing();
    else if (which == "RibRoundTrip")
        sectionRibRoundTrip();
    else if (which == "Network")
        sectionNetwork();
    else {
        std::printf("usage: test_bxdf_statement <LastBound|ColorSamples|ObjectBlock|Refusals|Parameters|NoKey|Instancing|RibRoundTrip|Network>\n");
        cldbl::unregisterBackend();
        return 2;
    }
    cldbl::unregisterBackend();
    return CL_RESULT(("Bxdf_" + which).c_str());
}

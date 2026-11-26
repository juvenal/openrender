/*
 * Spec 035, Phase 4 (T082), Bxdf_Tree_{diffuse,plastic,conductor,dielectric} and Bxdf_Tree_NoOsPremultiply (labels closures, bxdf): the four
 * built-in Bxdfs (contracts/bxdf.md "Built-ins", research R21) through their tables, with no renderer: findBxdf, create, build into an arena,
 * flatten, compare with the hand-built lobes of the same components and weights.
 *
 * The point has Cs = (0.8, 0.2, 0.1), Os, N = faceforwarded shading normal. Checked, per Bxdf: the lobes (ids, accumulated weights bit for bit,
 * parameter blocks field by field; alpha = roughness, not squared), the parameter table (names, defaults, ranges, onOutOfRange), that two builds
 * of equal inputs are bit-identical (FR-028), that `build` needs no renderer at all (declarative: it runs here with no shading context, no
 * services and no light), and for NoOsPremultiply that Os = (0.5, 0.5, 0.5) and Os = (1, 1, 1) build bit-identical trees (B1: the consumer
 * applies Oi = Os, as RISpec Appendix A does).
 *
 * Red on the starting tree: does not compile (bxdfAbi.h and bxdfRegistry.h do not exist).
 */
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "bxdfAbi.h"
#include "bxdfRegistry.h"
#include "bxdf_tree_support.h"
#include "test_support.h"

using namespace openrender::shading;

namespace {

    using namespace bxtree;

    // The values of a statement: every parameter of the table, defaulted, then the overrides by name
    struct Values {
            std::vector<F3> storage;
            std::vector<const void *> pointers;
    };
    Values valuesFor(const OrBxdfTable *t, const std::map<std::string, F3> &overrides) {
        Values v;
        v.storage.resize((size_t)t->numParams);
        for (int i = 0; i < t->numParams; ++i) {
            const OrBxdfParam &p = t->params[i];
            F3 value = {p.defaultValue[0], p.defaultValue[1], p.defaultValue[2]};
            const auto it = overrides.find(p.name);
            if (it != overrides.end())
                value = it->second;
            v.storage[(size_t)i] = value;
        }
        for (int i = 0; i < t->numParams; ++i)
            v.pointers.push_back(v.storage[(size_t)i].data());
        return v;
    }

    // create + build + destroy
    std::vector<Lobe> lobesOf(const OrBxdfTable *t, const Values &values, const OrBxdfPoint &point, bool &ok) {
        ok = false;
        OrBxdfData *data = t->create(values.pointers.data(), nullptr);
        if (data == nullptr)
            return {};
        std::vector<Lobe> out = lobesOfData(t, data, point, ok);
        t->destroy(data);
        return out;
    }

    const OrBxdfTable *find(const char *name) {
        const SBxdfLookup l = findBxdf(name);
        CL_CHECK(l.table != nullptr);
        return l.table;
    }
    const OrBxdfParam *param(const OrBxdfTable *t, const char *name) {
        for (int i = 0; i < t->numParams; ++i)
            if (std::strcmp(t->params[i].name, name) == 0)
                return &t->params[i];
        return nullptr;
    }
    // the parameter descriptor: type, default, range, onOutOfRange
    void checkParam(const OrBxdfTable *t, const char *name, OrBxdfParamType type, float def, float lo, float hi, OrBxdfRange onOut) {
        const OrBxdfParam *p = param(t, name);
        CL_CHECK(p != nullptr);
        if (p == nullptr)
            return;
        CL_CHECK(p->type == type);
        CL_CHECK_NEAR(p->defaultValue[0], def, 0.0);
        CL_CHECK_NEAR(p->minValue, lo, 0.0);
        CL_CHECK(hi < lo ? p->maxValue < p->minValue : p->maxValue == hi); // maxValue < minValue: no upper bound
        CL_CHECK(p->onOutOfRange == onOut);
        CL_CHECK(p->doc != nullptr && p->doc[0] != '\0');
    }

    void repeatable(const OrBxdfTable *t, const Values &v, const OrBxdfPoint &p, const std::vector<Lobe> &first) {
        bool ok = false;
        const std::vector<Lobe> again = lobesOf(t, v, p, ok);
        CL_CHECK(ok && sameLobes(first, again)); // FR-028: equal inputs, bit-identical trees
    }

    void sectionDiffuse() {
        const OrBxdfTable *t = find("diffuse");
        checkParam(t, "Kd", OR_BXDF_FLOAT, 1.0f, 0.0f, -1.0f, OR_BXDF_CLAMP);
        checkParam(t, "roughness", OR_BXDF_FLOAT, 0.0f, 0.0f, 1.0f, OR_BXDF_CLAMP);
        CL_CHECK(param(t, "Ka") == nullptr); // Ka is not a parameter: ambient is lighting work
        const OrBxdfPoint p = pointWith({1, 1, 1});
        bool ok;
        const std::vector<Lobe> a = lobesOf(t, valuesFor(t, {}), p, ok);
        CL_CHECK(ok && sameLobes(a, {diffuse(times(1.0f, kCs))}));
        repeatable(t, valuesFor(t, {}), p, a);
        const std::vector<Lobe> b = lobesOf(t, valuesFor(t, {{"Kd", {0.5f, 0, 0}}}), p, ok);
        CL_CHECK(ok && sameLobes(b, {diffuse(times(0.5f, kCs))}));
        // roughness > 0: Oren-Nayar, sigma = roughness
        const std::vector<Lobe> c = lobesOf(t, valuesFor(t, {{"roughness", {0.3f, 0, 0}}}), p, ok);
        CL_CHECK(ok && sameLobes(c, {orenNayar(times(1.0f, kCs), 0.3f)}));
    }

    void sectionPlastic() {
        const OrBxdfTable *t = find("plastic");
        checkParam(t, "Kd", OR_BXDF_FLOAT, 0.5f, 0.0f, -1.0f, OR_BXDF_CLAMP);
        checkParam(t, "Ks", OR_BXDF_FLOAT, 0.5f, 0.0f, -1.0f, OR_BXDF_CLAMP);
        checkParam(t, "roughness", OR_BXDF_FLOAT, 0.1f, 0.0f, 1.0f, OR_BXDF_CLAMP);
        checkParam(t, "specularcolor", OR_BXDF_COLOR, 1.0f, 0.0f, -1.0f, OR_BXDF_CLAMP);
        checkParam(t, "ior", OR_BXDF_FLOAT, 0.0f, 0.0f, -1.0f, OR_BXDF_REFUSE); // a negative ior refuses the statement
        const OrBxdfPoint p = pointWith({1, 1, 1});
        bool ok;
        // defaults: Kd .5 diffuse + Ks .5 microfacet (ggx, alpha = roughness .1 not squared, eta 0: no Fresnel term)
        const std::vector<Lobe> a = lobesOf(t, valuesFor(t, {}), p, ok);
        CL_CHECK(ok && sameLobes(a, {diffuse(times(0.5f, kCs)), ggx({0.5f, 0.5f, 0.5f}, 0.1f, 0.0f, 0)}));
        repeatable(t, valuesFor(t, {}), p, a);
        // ior 1.5 gives the lobe its dielectric Fresnel term; specularcolor tints it
        const std::vector<Lobe> b = lobesOf(t, valuesFor(t, {{"ior", {1.5f, 0, 0}}, {"specularcolor", {1.0f, 0.5f, 0.25f}}, {"Ks", {0.8f, 0, 0}}, {"roughness", {0.3f, 0, 0}}}), p, ok);
        CL_CHECK(ok && sameLobes(b, {diffuse(times(0.5f, kCs)), ggx(times(0.8f, {1.0f, 0.5f, 0.25f}), 0.3f, 1.5f, 0)}));
    }

    void sectionConductor() {
        const OrBxdfTable *t = find("conductor");
        checkParam(t, "Ks", OR_BXDF_FLOAT, 1.0f, 0.0f, -1.0f, OR_BXDF_CLAMP);
        checkParam(t, "roughness", OR_BXDF_FLOAT, 0.1f, 0.0f, 1.0f, OR_BXDF_CLAMP);
        const OrBxdfPoint p = pointWith({1, 1, 1});
        bool ok;
        const std::vector<Lobe> a = lobesOf(t, valuesFor(t, {}), p, ok);
        CL_CHECK(ok && sameLobes(a, {ggx(times(1.0f, kCs), 0.1f, 0.0f, 0)})); // Ks*Cs, eta 0 always
        repeatable(t, valuesFor(t, {}), p, a);
        const std::vector<Lobe> b = lobesOf(t, valuesFor(t, {{"Ks", {0.7f, 0, 0}}, {"roughness", {0.4f, 0, 0}}}), p, ok);
        CL_CHECK(ok && sameLobes(b, {ggx(times(0.7f, kCs), 0.4f, 0.0f, 0)}));
    }

    void sectionDielectric() {
        const OrBxdfTable *t = find("dielectric");
        checkParam(t, "ior", OR_BXDF_FLOAT, 1.5f, 0.0f, -1.0f, OR_BXDF_REFUSE); // (0, inf): at or below 0 refuses
        checkParam(t, "roughness", OR_BXDF_FLOAT, 0.0f, 0.0f, 1.0f, OR_BXDF_CLAMP);
        checkParam(t, "reflectColor", OR_BXDF_COLOR, 1.0f, 0.0f, -1.0f, OR_BXDF_CLAMP);
        checkParam(t, "transmitColor", OR_BXDF_COLOR, 1.0f, 0.0f, -1.0f, OR_BXDF_CLAMP);
        const OrBxdfPoint p = pointWith({1, 1, 1});
        bool ok;
        // the defaults: roughness 0 is below the alpha floor, so the pair degenerates to the delta lobes through Phase 2's floor: the tree is
        // still a reflect/transmit pair of microfacet components with alpha = roughness (the BSDF applies the floor)
        const std::vector<Lobe> a = lobesOf(t, valuesFor(t, {}), p, ok);
        CL_CHECK(ok && sameLobes(a, {ggx({1, 1, 1}, 0.0f, 1.5f, 0), ggx({1, 1, 1}, 0.0f, 1.5f, 1)}));
        repeatable(t, valuesFor(t, {}), p, a);
        const std::vector<Lobe> b = lobesOf(t, valuesFor(t, {{"roughness", {0.2f, 0, 0}}, {"reflectColor", {0.9f, 0.8f, 0.7f}}, {"transmitColor", {0.1f, 0.2f, 0.3f}}, {"ior", {1.33f, 0, 0}}}), p, ok);
        CL_CHECK(ok && sameLobes(b, {ggx({0.9f, 0.8f, 0.7f}, 0.2f, 1.33f, 0), ggx({0.1f, 0.2f, 0.3f}, 0.2f, 1.33f, 1)}));
    }

    // B1: the weights carry no Os
    void sectionNoOsPremultiply() {
        for (const char *name : {"diffuse", "plastic", "conductor", "dielectric"}) {
            const OrBxdfTable *t = find(name);
            bool okA, okB;
            const std::vector<Lobe> half = lobesOf(t, valuesFor(t, {}), pointWith({0.5f, 0.5f, 0.5f}), okA);
            const std::vector<Lobe> full = lobesOf(t, valuesFor(t, {}), pointWith({1, 1, 1}), okB);
            CL_CHECK(okA && okB && !half.empty() && sameLobes(half, full));
        }
    }

} // namespace

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "diffuse")
        sectionDiffuse();
    else if (which == "plastic")
        sectionPlastic();
    else if (which == "conductor")
        sectionConductor();
    else if (which == "dielectric")
        sectionDielectric();
    else if (which == "NoOsPremultiply")
        sectionNoOsPremultiply();
    else {
        std::printf("usage: test_bxdf_tree <diffuse|plastic|conductor|dielectric|NoOsPremultiply>\n");
        return 2;
    }
    return CL_RESULT(("Bxdf_Tree_" + which).c_str());
}

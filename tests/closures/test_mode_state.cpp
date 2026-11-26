/*
 * Spec 035, Phase 3 (T057), label closuremode: closuremode as a statement and as state (the four layers, FR-020; D-2; SC-008).
 * One program, one section per ctest (argv[1]).
 *
 *   Precedence      ClosureMode_Precedence: resolveClosureMode() (the attribute over the scene mode over `color`) for every pair; through
 *                   the state: Option "shade" "closuremode" and Attribute "shade" "closuremode" set, overridden and restored; the query
 *                   answers the string, "color" when unset; a context with no ClosureFrame answers scene mode Unset
 *   BadValue        ClosureMode_BadValue: a value outside the three spellings is CODE_RANGE (RIE_RANGE) naming the value and the three
 *                   spellings, and the previous value is kept, on the attribute side and the option side; an unknown token under Option
 *                   "shade" is CODE_BADTOKEN; Option "shaderformat" and Attribute "shade" "shaderformat" behave as before (D-2)
 *   Scope           ClosureMode_Scope: AttributeBegin/End, nested blocks, FrameBegin/End restoring the option (RISpec section 4),
 *                   TransformBegin (which does not save attributes), and an object instance taking the mode current at ObjectInstance, not
 *                   the definition's (Q-R2)
 *   SolidObject     ClosureMode_Scope_SolidObjectRISpec: closuremode restored after SolidEnd and ObjectEnd, as RISpec 3.2 section 4 says
 *   RibRoundTrip    ClosureMode_RibRoundTrip: parse, write with libRIBOut, parse again: both statements and their positions survive (SC-008)
 *
 * Red on the starting tree: does not compile (closureMode.h, closureFrame.h, EClosureMode, RI_CLOSUREMODE).
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "mode_support.h"
#include "attributes.h"
#include "camera.h"
#include "options.h"
#include "raytracer.h"
#include "renderer.h"
#include "rendererContext.h"
#include "riInterface.h"
#include "rib.h"
#include "stateVocabulary.h"

using namespace clmode;
using openrender::shading::ClosureFrame;
using openrender::shading::resolveClosureMode;

namespace {

    struct Diagnostic {
            int code, severity;
            std::string message;
    };
    std::vector<Diagnostic> g_diagnostics;
    void capture(RtInt code, RtInt severity, const char *message) { g_diagnostics.push_back({code, severity, message}); }

    COptions *options() { return CRenderer::context->getOptions(); } // RiGetContext is legal outside a frame or a world only
    CAttributes *attributes() { return CRenderer::context->getAttributes(FALSE); }

    // The query a shader's attribute("shade:closuremode") reaches: find() under the category "shade"
    template <typename T>
    bool findMode(const T *holder, std::string &out) {
        EVariableType type = TYPE_FLOAT;
        const void *value = nullptr;
        int intValue = 0;
        float floatValue = 0;
        if (!holder->find("closuremode", RI_SHADE, type, value, intValue, floatValue) || type != TYPE_STRING || value == nullptr)
            return false;
        out = static_cast<const char *>(value);
        return true;
    }

    void attributeMode(const char *m) {
        RtString v = const_cast<RtString>(m);
        RiAttribute(RI_SHADE, RI_CLOSUREMODE, &v, RI_NULL);
    }
    void optionMode(const char *m) {
        RtString v = const_cast<RtString>(m);
        RiOption(RI_SHADE, RI_CLOSUREMODE, &v, RI_NULL);
    }
    EClosureMode attributeValue() { return attributes()->closureMode; }
    EClosureMode optionValue() { return options()->closureMode; }

    // ---- Precedence --------------------------------------------------------------------------------------------------------
    void sectionPrecedence() {
        const ClosureMode all[] = {ClosureMode::Unset, ClosureMode::Color, ClosureMode::Both, ClosureMode::Closures};
        for (ClosureMode attribute : all)
            for (ClosureMode scene : all) {
                const ClosureFrame frame(scene);
                const ClosureMode want = attribute != ClosureMode::Unset ? attribute : (scene != ClosureMode::Unset ? scene : ClosureMode::Color);
                CL_CHECK((int)resolveClosureMode(attribute, &frame) == (int)want);
            }
        for (ClosureMode attribute : all) // a context with no frame: scene mode Unset
            CL_CHECK((int)resolveClosureMode(attribute, nullptr) == (int)(attribute != ClosureMode::Unset ? attribute : ClosureMode::Color));

        RiBegin(RI_NULL);
        RiErrorHandler(capture);
        std::string text;
        CL_CHECK(optionValue() == EClosureMode::Unset && attributeValue() == EClosureMode::Unset);
        CL_CHECK(findMode(attributes(), text) && text == "color"); // unset answers "color"
        CL_CHECK(findMode(options(), text) && text == "color");
        optionMode("both");
        CL_CHECK(optionValue() == EClosureMode::Both);
        CL_CHECK(findMode(options(), text) && text == "both");
        CL_CHECK(attributeValue() == EClosureMode::Unset); // the option does not set the attribute
        RiAttributeBegin();
        attributeMode("closures");
        CL_CHECK(attributeValue() == EClosureMode::Closures);
        CL_CHECK(findMode(attributes(), text) && text == "closures");
        CL_CHECK((int)resolveClosureMode((ClosureMode)attributeValue(), nullptr) == (int)ClosureMode::Closures);
        RiAttributeEnd();
        CL_CHECK(attributeValue() == EClosureMode::Unset); // removing the attribute falls back to the option, then to color
        CL_CHECK(findMode(attributes(), text) && text == "color");
        CL_CHECK(optionValue() == EClosureMode::Both);
        RiEnd();

        // a context with no ClosureFrame (an imager's, a double's) answers no frame: scene mode Unset
        RiBegin(RI_NULL);
        RiWorldBegin();
        {
            class CStubCamera final : public openrender::ICamera {
                public:
                    openrender::CameraRay generateRay(float, float, float, const float[2]) const override {
                        openrender::CameraRay r = {};
                        r.dir[2] = 1;
                        return r;
                    }
            } camera;
            CRaytracer *rt = new CRaytracer(0, camera);
            CL_CHECK(rt->shadingContext().closureFrame() == nullptr);
            delete rt;
        }
        RiWorldEnd();
        RiEnd();
        CL_CHECK(g_diagnostics.empty());
    }

    // ---- BadValue ----------------------------------------------------------------------------------------------------------
    void sectionBadValue() {
        RiBegin(RI_NULL);
        RiErrorHandler(capture);
        g_diagnostics.clear();
        attributeMode("both");
        attributeMode("all"); // outside the three spellings
        CL_CHECK(g_diagnostics.size() == 1);
        if (!g_diagnostics.empty()) {
            CL_CHECK(g_diagnostics[0].code == RIE_RANGE);
            const std::string &m = g_diagnostics[0].message;
            CL_CHECK(m.find("all") != std::string::npos && m.find("color") != std::string::npos && m.find("both") != std::string::npos && m.find("closures") != std::string::npos);
        }
        CL_CHECK(attributeValue() == EClosureMode::Both); // the previous value is kept

        g_diagnostics.clear();
        optionMode("closures");
        optionMode("all");
        CL_CHECK(g_diagnostics.size() == 1 && g_diagnostics[0].code == RIE_RANGE);
        CL_CHECK(optionValue() == EClosureMode::Closures); // and on the option side

        g_diagnostics.clear();
        int one = 1;
        RiOption(RI_SHADE, (RtToken) "int bogus", &one, RI_NULL); // an unknown token under Option "shade"
        CL_CHECK(g_diagnostics.size() == 1 && g_diagnostics[0].code == RIE_BADTOKEN);

        // D-2: shaderformat behaves as before
        g_diagnostics.clear();
        RtString slo = (RtString) "slo";
        RiAttribute(RI_SHADE, RI_SHADERFORMAT, &slo, RI_NULL);
        RiOption(RI_SHADERFORMAT, "default", &slo, RI_NULL);
        CL_CHECK(g_diagnostics.empty());
        RiEnd();
    }

    // ---- Scope -------------------------------------------------------------------------------------------------------------
    void sectionScope() {
        RiBegin(RI_NULL);
        RiErrorHandler(capture);
        // nested attribute blocks
        attributeMode("both");
        RiAttributeBegin();
        CL_CHECK(attributeValue() == EClosureMode::Both); // a block inherits
        attributeMode("closures");
        RiAttributeBegin();
        CL_CHECK(attributeValue() == EClosureMode::Closures);
        attributeMode("color");
        CL_CHECK(attributeValue() == EClosureMode::Color);
        RiAttributeEnd();
        CL_CHECK(attributeValue() == EClosureMode::Closures);
        RiAttributeEnd();
        CL_CHECK(attributeValue() == EClosureMode::Both);

        // TransformBegin does not save attributes: a setting made inside survives its end
        RiTransformBegin();
        attributeMode("closures");
        RiTransformEnd();
        CL_CHECK(attributeValue() == EClosureMode::Closures);
        attributeMode("both");

        // FrameBegin/FrameEnd restore the option (RISpec section 4)
        optionMode("color");
        RiFrameBegin(1);
        optionMode("closures");
        CL_CHECK(optionValue() == EClosureMode::Closures);
        RiFrameEnd();
        CL_CHECK(optionValue() == EClosureMode::Color);

        // the other begin-end blocks that save attributes: WorldBegin/End is a frame's; SolidBegin/End and ObjectBegin/End restore them
        RiWorldBegin();
        attributeMode("both");
        RiWorldEnd();
        RiEnd();
        CL_CHECK(g_diagnostics.empty());

        // an object instance takes the mode current at ObjectInstance, not the definition's (Q-R2): defined under `both`, instanced under
        // `closures` it is black and delivers trees as a closures object; defined under `closures`, instanced with no mode it is a color object
        for (int variant = 0; variant < 2; ++variant) {
            SScene s;
            s.surface = "cl_cap";
            s.noFront = true;
            s.consumer = true;
            s.extra = [variant]() {
                RtPoint quad[4] = {{-4, -4, 1}, {4, -4, 1}, {4, 4, 1}, {-4, 4, 1}};
                RtColor c = {0.25f, 0.5f, 0.75f};
                RiColor(c);
                RiSurface("cl_cap_rslo", RI_NULL);
                RiAttributeBegin();
                attributeMode(variant == 0 ? "both" : "closures"); // the definition's mode
                RtObjectHandle h = RiObjectBegin();
                RiPolygon(4, RI_P, (RtPointer)quad, RI_NULL);
                RiObjectEnd();
                RiAttributeEnd();
                RiAttributeBegin();
                if (variant == 0)
                    attributeMode("closures"); // the instance's mode
                RiObjectInstance(h);
                RiAttributeEnd();
            };
            const SRun r = render(s);
            if (variant == 0) {
                CL_CHECK(r.modes.size() == 1 && r.modes.count((int)ClosureMode::Closures) == 1);
                CL_CHECK(r.image.at(3, 3, 0) == 0.0f); // closures: Ci 0
            }
            else {
                CL_CHECK(r.trees == 0);                // the instance is a color object
                CL_CHECK(r.image.at(3, 3, 0) > 0.0f);
            }
        }
    }

    // ---- SolidObject -------------------------------------------------------------------------------------------------------
    // RISpec 3.2 section 4: every begin-end pair except Transform and Motion saves the attributes at its begin and restores them at its end, so
    // closuremode set inside SolidBegin or ObjectBegin must be the enclosing value again after SolidEnd or ObjectEnd.
    void sectionSolidObject() {
        RiBegin(RI_NULL);
        RiErrorHandler(capture);
        RiWorldBegin();
        attributeMode("both");
        RiSolidBegin(RI_PRIMITIVE);
        attributeMode("closures");
        CL_CHECK(attributeValue() == EClosureMode::Closures);
        RiSolidEnd();
        CL_CHECK(attributeValue() == EClosureMode::Both); // restored by SolidEnd (RISpec section 4)
        RtObjectHandle object = RiObjectBegin();
        attributeMode("closures");
        RiObjectEnd();
        CL_CHECK(attributeValue() == EClosureMode::Both); // restored by ObjectEnd (RISpec section 4)
        (void)object;
        RiWorldEnd();
        RiEnd();
    }

    // ---- RibRoundTrip ------------------------------------------------------------------------------------------------------
    struct Modes {
            EClosureMode option, attribute;
            bool operator==(const Modes &o) const { return option == o.option && attribute == o.attribute; }
    };
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
    Modes parseWithRenderer(const std::string &rib) {
        RiBegin(RI_NULL);
        RiReadArchive((RtString)rib.c_str(), nullptr, RI_NULL);
        const Modes m{options()->closureMode, attributes()->closureMode};
        RiEnd();
        return m;
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
        char tmpl[] = "/tmp/closuremode_roundtrip_XXXXXX";
        const std::string dir = mkdtemp(tmpl);
        const std::string declared = dir + "/declared.rib", inlined = dir + "/inline.rib", written = dir + "/written.rib", writtenInline = dir + "/written-inline.rib", again = dir + "/again.rib";
        writeFile(declared, "Option \"shade\" \"closuremode\" [\"both\"]\nAttribute \"shade\" \"closuremode\" [\"closures\"]\n");
        writeFile(inlined, "Option \"shade\" \"string closuremode\" [\"both\"]\nAttribute \"shade\" \"string closuremode\" [\"closures\"]\n");
        const Modes expected{EClosureMode::Both, EClosureMode::Closures};
        CL_CHECK(parseWithRenderer(declared) == expected);
        CL_CHECK(parseWithRenderer(inlined) == expected);

        parseIntoWriter(declared, written);
        parseIntoWriter(inlined, writtenInline);
        for (const std::string &path : {written, writtenInline}) {
            const std::string text = slurp(path);
            const size_t option = text.find("Option \"shade\""), attribute = text.find("Attribute \"shade\"");
            CL_CHECK(option != std::string::npos && attribute != std::string::npos);
            CL_CHECK(option < attribute); // the positions are kept
            CL_CHECK(text.find("closuremode") != std::string::npos && text.find("both") != std::string::npos && text.find("closures") != std::string::npos);
        }
        CL_CHECK(parseWithRenderer(written) == expected);
        CL_CHECK(parseWithRenderer(writtenInline) == expected);
        parseIntoWriter(written, again); // the writer's output is stable
        CL_CHECK(slurp(again) == slurp(written));
        for (const std::string &p : {declared, inlined, written, writtenInline, again})
            unlink(p.c_str());
        rmdir(dir.c_str());
    }

} // namespace

int main(int argc, char **argv) {
    cldbl::registerBackend();
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "Precedence")
        sectionPrecedence();
    else if (which == "BadValue")
        sectionBadValue();
    else if (which == "Scope")
        sectionScope();
    else if (which == "SolidObject")
        sectionSolidObject();
    else if (which == "RibRoundTrip")
        sectionRibRoundTrip();
    else {
        std::printf("usage: test_mode_state <Precedence|BadValue|Scope|RibRoundTrip>\n");
        cldbl::unregisterBackend();
        return 2;
    }
    cldbl::unregisterBackend();
    return CL_RESULT(("ClosureMode_" + which).c_str());
}

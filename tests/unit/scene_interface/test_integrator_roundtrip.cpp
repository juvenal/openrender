/**
 * Project: openRender
 *
 * File: test_integrator_roundtrip.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), US3 AS2, FR-019. RibOut_IntegratorRoundTrip.
 *
 *   Parse -> RIB writer preserves the Integrator statement and its parameters (inline declarations are written as
 *   declared, like Hider's); the writer's own output parses back to the same options and is stable (writing what
 *   it wrote gives the same text); the preview back-end, which has no use for an integrator, ignores the statement
 *   silently (no diagnostic). Modelled on test_sampling_option_roundtrip.cpp (the parser is set up by hand for the
 *   writer, which never goes through CRenderer::beginRenderer()).
 *
 *   Red by assertion / compile error until Phase 7 (no Integrator statement, no options members).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "common/diagnostics.h"
#include "common/memory.h"
#include "options.h"
#include "renderer.h"
#include "ri.h"
#include "ribpreview_api.h"
#include "riInterface.h"
#include "rib.h"

static int failures = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

namespace {

    struct Snapshot {
            std::string integrator;
            std::vector<std::string> names;
            std::vector<int> types;
            std::vector<int> items;
            int firstInt = 0;
            float firstFloat = 0;
            std::string firstString;
            bool operator==(const Snapshot &o) const {
                return integrator == o.integrator && names == o.names && types == o.types && items == o.items && firstInt == o.firstInt &&
                       firstFloat == o.firstFloat && firstString == o.firstString;
            }
    };

    class CCountingSink : public CDiagnosticSink {
        public:
            int reports = 0;
            void report(EErrorCode, EDiagnosticSeverity, const char *) override { ++reports; }
    };

    void writeFile(const std::string &path, const std::string &text) {
        std::ofstream f(path);
        f << text;
    }

    std::string slurp(const std::string &path) {
        std::ifstream f(path);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    // Parse with a renderer context and read the integrator choice back.
    Snapshot parseWithRenderer(const std::string &rib) {
        RiBegin(RI_NULL);
        RiReadArchive((RtString)rib.c_str(), nullptr, RI_NULL);
        const COptions *o = static_cast<CRiInterface *>(const_cast<void *>(RiGetContext()))->getOptions();
        Snapshot s;
        s.integrator = o->integrator != nullptr ? o->integrator : "";
        for (int i = 0; i < o->numIntegratorParameters; ++i) {
            const auto &p = o->integratorParameters[i];
            s.names.push_back(p.name);
            s.types.push_back(p.type);
            s.items.push_back(p.numItems);
            if (std::strcmp(p.name, "maxdepth") == 0)
                s.firstInt = static_cast<const int *>(p.data)[0];
            if (std::strcmp(p.name, "gain") == 0)
                s.firstFloat = static_cast<const float *>(p.data)[0];
            if (std::strcmp(p.name, "mode") == 0)
                s.firstString = static_cast<const char *const *>(p.data)[0];
        }
        RiEnd();
        return s;
    }

    // Parse straight into the RIB writer.
    void parseIntoWriter(const std::string &rib, const std::string &out) {
        RiBegin((RtToken)out.c_str());
        CRenderer::initDeclarations();
        globalArena.init();
        ribParse(rib.c_str(), nullptr);
        globalArena.tini();
        CRenderer::shutdownDeclarations();
        RiEnd();
    }

} // namespace

int main() {
    char tmpl[] = "/tmp/integrator_roundtrip_XXXXXX";
    const std::string dir = mkdtemp(tmpl);
    const std::string inlined = dir + "/inline.rib", declared = dir + "/declared.rib", written = dir + "/written.rib",
                      writtenDeclared = dir + "/written-declared.rib", again = dir + "/again.rib";

    writeFile(inlined,
              "Option \"limits\" \"bucketsize\" [16 16]\n"
              "Integrator \"classic\" \"int maxdepth\" [5] \"float gain\" [0.5] \"string mode\" [\"fast\"]\n");
    writeFile(declared,
              "Declare \"maxdepth\" \"int\"\n"
              "Declare \"gain\" \"float\"\n"
              "Declare \"mode\" \"string\"\n"
              "Integrator \"classic\" \"maxdepth\" [5] \"gain\" [0.5] \"mode\" [\"fast\"]\n");

    // 1. The parser stores the same choice for both spellings.
    const Snapshot a = parseWithRenderer(inlined);
    const Snapshot b = parseWithRenderer(declared);
    CHECK(a.integrator == "classic");
    CHECK(a.names.size() == 3 && a.firstInt == 5 && a.firstFloat == 0.5f && a.firstString == "fast");
    CHECK(a == b);

    // 2. Parser -> RIB writer: the statement and its parameters are written.
    parseIntoWriter(inlined, written);
    parseIntoWriter(declared, writtenDeclared);
    for (const std::string &path : {written, writtenDeclared}) {
        const std::string text = slurp(path);
        std::fprintf(stderr, "%s:\n%s\n", path.c_str(), text.c_str());
        CHECK(text.find("Integrator \"classic\"") != std::string::npos);
        CHECK(text.find("maxdepth") != std::string::npos);
        CHECK(text.find("gain") != std::string::npos);
        CHECK(text.find("mode") != std::string::npos);
        CHECK(text.find("fast") != std::string::npos);
    }
    CHECK(slurp(written).find("bucketsize") != std::string::npos); // control: other statements are untouched

    // 3. RIB writer -> parser: the re-parsed choice equals the original.
    CHECK(parseWithRenderer(written) == a);
    CHECK(parseWithRenderer(writtenDeclared) == a);

    // The writer's output is stable.
    parseIntoWriter(written, again);
    CHECK(slurp(again) == slurp(written));

    // 4. The preview back-end ignores the statement silently.
    {
        CCountingSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);
        PreviewSceneC *scene = ribpreview_load(inlined.c_str());
        installDiagnosticSink(previous);
        CHECK(scene != nullptr);
        CHECK(sink.reports == 0);
        if (scene != nullptr)
            ribpreview_free(scene);
    }

    for (const std::string &p : {inlined, declared, written, writtenDeclared, again})
        unlink(p.c_str());
    rmdir(dir.c_str());

    if (failures) {
        std::fprintf(stderr, "FAILED: %d check(s)\n", failures);
        return 1;
    }
    return 0;
}

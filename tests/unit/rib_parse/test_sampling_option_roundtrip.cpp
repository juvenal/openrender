/**
 * Project: openRender
 *
 * File: test_sampling_option_roundtrip.cpp
 *
 * Description:
 *   Spec 039, Phase 3 (US5), SamplingOption_RibRoundTrip (FR-027): a RIB that
 *   sets Option "sampling" survives being parsed, written by the RIB writer
 *   (libRIBOut) and parsed again, with the options equal at each end. In
 *   process; each session resets the lexer (riblex_destroy(), CLAUDE.md
 *   gotcha 20) so that consecutive sessions in one process are safe.
 *
 *   The first input names the tokens as pre-declared (so it also needs the
 *   pre-declaration of "sampler" and "seed"), the second declares them inline.
 *   A bucketsize option is written alongside as a control: it must survive the
 *   same path, so a failure confined to the sampling tokens is not the
 *   harness.
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
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "common/memory.h"
#include "common/sampling.h"
#include "options.h"
#include "renderer.h"
#include "ri.h"
#include "riInterface.h"
#include "rib.h"

namespace sp = openrender::sampling;

static int failures = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

struct Snapshot {
        sp::Backend backend;
        uint32_t seed;
        int bucketWidth, bucketHeight;
        bool operator==(const Snapshot &o) const {
            return backend == o.backend && seed == o.seed && bucketWidth == o.bucketWidth && bucketHeight == o.bucketHeight;
        }
};

static void writeFile(const std::string &path, const std::string &text) {
    std::ofstream f(path);
    f << text;
}

static std::string slurp(const std::string &path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Parse a RIB with a renderer context and read the options back.
static Snapshot parseWithRenderer(const std::string &rib) {
    RiBegin(RI_NULL);
    RiReadArchive((RtString)rib.c_str(), nullptr, RI_NULL);
    const COptions *o = static_cast<CRiInterface *>(const_cast<void *>(RiGetContext()))->getOptions();
    const Snapshot s{o->samplingBackend, o->samplingSeed, o->bucketWidth, o->bucketHeight};
    RiEnd();
    return s;
}

// Parse a RIB straight into the RIB writer.
static void parseIntoWriter(const std::string &rib, const std::string &out) {
    RiBegin((RtToken)out.c_str());
    // The RIB writer never goes through CRenderer::beginRenderer(), so the
    // parser's declarations and arena are set up by hand (as RiBegin does
    // for .orenderrc).
    CRenderer::initDeclarations();
    globalArena.init();
    ribParse(rib.c_str(), nullptr);
    globalArena.tini();
    CRenderer::shutdownDeclarations();
    RiEnd();
}

int main() {
    char tmpl[] = "/tmp/sampling_roundtrip_XXXXXX";
    const std::string dir = mkdtemp(tmpl);
    const std::string declared = dir + "/declared.rib", inlined = dir + "/inline.rib", written = dir + "/written.rib",
                      writtenInline = dir + "/written-inline.rib";

    writeFile(declared,
              "Option \"limits\" \"bucketsize\" [16 16]\n"
              "Option \"sampling\" \"sampler\" [\"pmj02\"]\n"
              "Option \"sampling\" \"seed\" [-7]\n");
    writeFile(inlined,
              "Option \"limits\" \"bucketsize\" [16 16]\n"
              "Option \"sampling\" \"string sampler\" [\"pmj02\"]\n"
              "Option \"sampling\" \"int seed\" [-7]\n");

    const Snapshot expected{sp::Backend::PMJ02, 4294967289u, 16, 16};

    // 1. The parser accepts both spellings and stores the same options.
    const Snapshot a = parseWithRenderer(declared);
    const Snapshot b = parseWithRenderer(inlined);
    CHECK(a == expected);
    CHECK(b == expected);

    // 2. Parser -> RIB writer: both tokens are written out.
    parseIntoWriter(declared, written);
    parseIntoWriter(inlined, writtenInline);
    for (const std::string &path : {written, writtenInline}) {
        const std::string text = slurp(path);
        std::fprintf(stderr, "%s:\n%s\n", path.c_str(), text.c_str());
        CHECK(text.find("bucketsize") != std::string::npos); // control
        CHECK(text.find("Option \"sampling\"") != std::string::npos);
        CHECK(text.find("sampler") != std::string::npos);
        CHECK(text.find("pmj02") != std::string::npos);
        CHECK(text.find("seed") != std::string::npos);
        CHECK(text.find("-7") != std::string::npos);
    }

    // 3. RIB writer -> parser: the re-parsed options equal the original ones.
    CHECK(parseWithRenderer(written) == a);
    CHECK(parseWithRenderer(writtenInline) == a);

    // The writer's output is stable: writing what it wrote gives the same text.
    const std::string again = dir + "/again.rib";
    parseIntoWriter(written, again);
    CHECK(slurp(again) == slurp(written));

    for (const std::string &p : {declared, inlined, written, writtenInline, again})
        unlink(p.c_str());
    rmdir(dir.c_str());

    if (failures) {
        std::fprintf(stderr, "FAILED: %d check(s)\n", failures);
        return 1;
    }
    return 0;
}

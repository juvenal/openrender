/**
 * Project: openRender
 *
 * File: test_writer_text.cpp
 *
 * Description:
 *   Spec 038 (Scene Interface untangle), SceneInterface_WriterText (US1, a characterization): what the RIB
 *   writer writes, and what the parser reports while it does, for each RIB under tests/RIB/writer/. Green on
 *   the starting tree by design, and it must stay green when the grammar and the lexer stop naming the
 *   renderer core (Phase 2): the same declared and inline parameters, the same "Parameter ... is not
 *   declared" errors at the same statements, the same ReadArchive lookups.
 *
 *   Each case runs in its own scratch directory that holds a copy of the case as input.rib and of everything
 *   under tests/RIB/writer/include/ (so a bare archive name is found in the working directory, and
 *   sub/nested_inc.rib by its separator). The case is written through RiBegin("out.rib"), the way a client of
 *   the C API reaches the writer, with the parser's declarations and arena set up by hand as the other writer
 *   tests do (test_sampling_option_roundtrip.cpp): the writer never goes through beginRenderer().
 *
 *   Compared with reference/writer/<case>.rib (the written text, its Creator and CreationDate lines
 *   normalized) and reference/writer/<case>.diag (every diagnostic the session reported, one per line, in
 *   order). Usage: test_scene_interface_writer_text <ribs dir> <reference dir> <scratch dir> [--capture [--recapture] | --capture-differences]
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

#include "common/memory.h"
#include "renderer.h"
#include "ri.h"
#include "riInterface.h"
#include "rib.h"
#include "scene_interface_testing.h"

namespace fs = std::filesystem;

// The two lines of the writer's header that differ from run to run.
static std::string normalizeWritten(const std::string &text) {
    std::string out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("##CreationDate", 0) == 0)
            line = "##CreationDate <date>";
        else if (line.rfind("##Creator", 0) == 0)
            line = "##Creator openRender <version>";
        out += line + "\n";
    }
    return out;
}

static void runCase(const fs::path &input, const fs::path &ribDir, const sitest::RefLayout &refs, const fs::path &work, const sitest::Options &opt) {
    const std::string name = input.stem().string();
    const fs::path scratch = work / name;
    std::error_code ec;
    fs::remove_all(scratch, ec);
    fs::create_directories(scratch);
    if (fs::exists(ribDir / "include"))
        fs::copy(ribDir / "include", scratch, fs::copy_options::recursive);
    fs::copy_file(input, scratch / "input.rib", fs::copy_options::overwrite_existing);

    char previousDirectory[4096];
    SI_CHECK(getcwd(previousDirectory, sizeof(previousDirectory)) != nullptr);
    SI_CHECK(chdir(scratch.c_str()) == 0);

    sitest::CRecordingSink sink;
    RiBegin((RtToken) "out.rib");
    CDiagnosticSink *attached = installDiagnosticSink(&sink);

    // The RIB writer never goes through CRenderer::beginRenderer(), so the declarations and the arena are set up by hand
    CRenderer::initDeclarations();
    globalArena.init();
    ribParse("input.rib", nullptr);
    globalArena.tini();
    CRenderer::shutdownDeclarations();

    // The context's own diagnostics routing is put back before RiEnd, which detaches it
    installDiagnosticSink(attached);
    RiEnd();

    SI_CHECK(chdir(previousDirectory) == 0);

    const std::string written = normalizeWritten(sitest::readFile((scratch / "out.rib").string()));
    SI_CHECK_MSG(!written.empty(), "%s: the writer produced no output", name.c_str());
    sitest::compareOrCapture(refs, "writer/" + name + ".rib", written, opt, name + " (written text)");
    sitest::compareOrCapture(refs, "writer/" + name + ".diag", sink.text(), opt, name + " (diagnostics)");
}

int main(int argc, char **argv) {
    const sitest::Options opt = sitest::takeOptions(argc, argv);
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s <ribs dir> <reference dir> <scratch dir> [--capture [--recapture] | --capture-differences]\n", argv[0]);
        return 2;
    }
    const fs::path ribDir = argv[1], work = argv[3];
    const sitest::RefLayout refs{argv[2], ""};
    fs::create_directories(work);

    std::vector<fs::path> cases;
    for (const auto &entry : fs::directory_iterator(ribDir))
        if (entry.is_regular_file() && entry.path().extension() == ".rib")
            cases.push_back(entry.path());
    std::sort(cases.begin(), cases.end());
    SI_CHECK_MSG(!cases.empty(), "no .rib case under %s", ribDir.c_str());

    for (const fs::path &c : cases)
        runCase(c, ribDir, refs, fs::absolute(work), opt);

    if (sitest::failures() != 0) {
        std::fprintf(stderr, "SceneInterface_WriterText: %d failure(s)\n", sitest::failures());
        return 1;
    }
    std::printf("SceneInterface_WriterText: %zu case(s)\n", cases.size());
    return 0;
}

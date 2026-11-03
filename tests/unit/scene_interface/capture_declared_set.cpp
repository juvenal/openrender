/**
 * Project: openRender
 *
 * File: capture_declared_set.cpp
 *
 * Description:
 *   Spec 038 (Scene Interface untangle), the declared-set reference: a small program, linked with ribRender,
 *   that runs the unmodified CRenderer::initDeclarations() and writes the table it built to
 *   reference/declared_set.txt:
 *
 *     variables N                       the length of the variable list
 *     v <index> <name> type=.. container=.. storage=.. entry=.. numFloats=.. numItems=.. marker=0x..
 *                                       every variable, in list order (the head is the last one declared)
 *     globals M                         the global count
 *     g <index> <name> entry=..         the global array, in numbering order
 *
 *   SceneInterface_Services (Phase 2) compares the table of a standard session and of a renderer session with
 *   this file, so both are pinned to the starting tree's table and not to each other (FR-002: the declared
 *   set, the global numbering and every entry index exactly as today). It is a program, not a test: the
 *   build target scene-interface-capture-baselines runs it with --capture, and it refuses to replace an
 *   existing reference unless --recapture is also given.
 *   Usage: capture_scene_interface_declared_set <reference dir> --capture [--recapture]
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
#include <string>

#include "common/memory.h"
#include "declarations.h"
#include "renderer.h"
#include "scene_interface_testing.h"

int main(int argc, char **argv) {
    const sitest::Options opt = sitest::takeOptions(argc, argv);
    if (argc != 2 || !(opt.capture || opt.onlyDifferences)) {
        std::fprintf(stderr, "usage: %s <reference dir> --capture [--recapture]\n", argv[0]);
        return 2;
    }

    globalArena.init();
    CRenderer::initDeclarations();

    const std::string text = sitest::declaredSetText(CDeclarations::variableList(), CDeclarations::numGlobals(), [](int i) {
        return static_cast<const CVariable *>(CDeclarations::global(i));
    });

    CRenderer::shutdownDeclarations();
    globalArena.tini();

    sitest::compareOrCapture(sitest::RefLayout{argv[1], ""}, "declared_set.txt", text, opt, "declared set");
    return sitest::failures() == 0 ? 0 : 1;
}

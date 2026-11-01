/**
 * Project: openRender
 *
 * File: test_parser_reentrant.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), ShadeopAbi_ParserReentrant (FR-012, FR-037, US4-3).
 *
 *   parseShadeopDeclaration keeps no state between calls and none shared between threads. Eight
 *   threads each make 10,000 parses over the table of declarations.txt (the table the approved
 *   ShadeopAbi_DeclarationCharacterization fixes), each thread starting at another case so that
 *   at every moment the threads are parsing different declarations; every result must be the one
 *   the table says: accepted or not, the name and the prototype. All threads start together.
 *   The old dsoParse keeps its result in file statics and is not safe to call like this.
 *
 *   argv: <declarations.txt>.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "declaration_table.h"
#include "shadeop/shadeopDeclaration.h"

namespace {
    constexpr int kThreads = 8;
    constexpr int kParsesPerThread = 10000;
} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_parser_reentrant <declarations.txt>\n");
        return 2;
    }
    std::vector<SDeclarationCase> cases;
    if (!declaration_table::load(argv[1], cases)) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::atomic<int> wrong{0};
    std::mutex reportMutex;
    std::string firstWrong;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            ++ready;
            while (!go.load())
                std::this_thread::yield();
            for (int k = 0; k < kParsesPerThread; ++k) {
                const SDeclarationCase &c = cases[(size_t)(t * 13 + k) % cases.size()];
                SShadeopDeclaration out;
                const bool accepted = parseShadeopDeclaration(c.declaration, out);
                const bool ok = accepted == c.accepted && (!accepted || (out.name == c.name && out.prototype == c.prototype));
                if (!ok && wrong.fetch_add(1) == 0) {
                    std::lock_guard<std::mutex> lock(reportMutex);
                    firstWrong = "thread " + std::to_string(t) + " parse " + std::to_string(k) + " of [" + c.declaration.substr(0, 60) + "]: expected " +
                                 (c.accepted ? "accepted " + c.name.substr(0, 20) + " " + c.prototype.substr(0, 20) : std::string("rejected")) + ", got " +
                                 (accepted ? "accepted " + out.name.substr(0, 20) + " " + out.prototype.substr(0, 20) : std::string("rejected"));
                }
            }
        });
    }
    while (ready.load() < kThreads)
        std::this_thread::yield();
    go.store(true);
    for (std::thread &t : threads)
        t.join();

    if (wrong.load() != 0) {
        printf("FAIL: %d of %d parses gave another result than the table; first: %s\n", wrong.load(), kThreads * kParsesPerThread, firstWrong.c_str());
        return 1;
    }
    printf("ok: %d threads x %d parses, all as the table says\n", kThreads, kParsesPerThread);
    return 0;
}

/**
 * Project: openRender
 *
 * File: test_services.cpp
 *
 * Description:
 *   Spec 038 (Scene Interface untangle), SceneInterface_Services (US1, US2; contracts/scene-services.md).
 *   Written before the code: it uses the Scene-owned declaration and file-search services
 *   (parse/riServices.h), the Graphics State declaration table (state/declarations.h) and the `if` evaluator
 *   as a Scene function (parse/riCondition.h), none of which exist on the starting tree. Registered DISABLED
 *   with an EXCLUDE_FROM_ALL executable that does not compile there; that failure is its red evidence
 *   (FR-029, decision A). Phase 2 enables it.
 *
 *   The cases, in the order the executable runs them (the order matters, see 3):
 *   1. No service installed: riRetrieveVariable answers nullptr; riLocateFile finds only a name with a
 *      directory separator that exists, and a bare name in the working directory is not found.
 *   2. A lexer `ReadArchive` lookup with no active context (renderMan == NULL; rib.l reads
 *      renderMan->getOptions() unchecked today) neither crashes nor finds a file through a search path.
 *   3. A standard session (CRiStandardSession): the declared set equals reference/declared_set.txt, which the
 *      unmodified CRenderer::initDeclarations() produced (capture_declared_set.cpp), so the standard set is
 *      pinned to the starting tree and not to the renderer's; and the standard file search finds the same
 *      files, with the same `info` lines and results, as CRenderer::locateFile outside a network session.
 *      That comparison holds only in a process with no earlier renderer session (decision D4-a:
 *      CRenderer::temporaryPath is never cleared at RiEnd, GitHub #136), so it runs here, before the first
 *      renderer session of this executable.
 *   4. Nested sessions restore the outer services (including services that were installed by hand).
 *   5. riEvaluateCondition on a context whose getAttributes/getOptions answer nullptr reports "not found"
 *      without crashing (ifexpr.y:203-224 calls find on them unchecked today).
 *   6. A renderer session (RiBegin / RiEnd): its declared set also equals the reference.
 *   7. D4-a: after that renderer session a bare file name that exists only in the working directory is found
 *      by riLocateFile under a standard session, with the info line "name" -> "name". The case asserts only
 *      the standard search, never the core's temporaryPath behaviour, so a later fix of GitHub #136 does not
 *      break it.
 *
 *   Usage: test_scene_interface_services <reference dir> <scratch dir>
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
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

#include "common/memory.h"
#include "common/os.h"
#include "declarations.h"
#include "options.h"
#include "renderer.h"
#include "ri.h"
#include "riCondition.h"
#include "riInterface.h"
#include "riServices.h"
#include "rib.h"
#include "scene_interface_testing.h"

namespace fs = std::filesystem;

namespace {

    fs::path g_scratch;
    std::string g_declaredSetReference;

    void touch(const fs::path &path, const char *text = "Sphere 1 -1 1 360\n") {
        fs::create_directories(path.parent_path());
        sitest::writeFile(path.string(), text);
    }

    // The declared set seen through the Graphics State table, in the format of the reference
    std::string declaredSetOfTable() {
        const std::string head = sitest::declaredSetHead(g_declaredSetReference);
        const CVariable *first = head.empty() ? nullptr : CDeclarations::find(head.c_str());
        return sitest::declaredSetText(first, CDeclarations::numGlobals(), [](int i) { return static_cast<const CVariable *>(CDeclarations::global(i)); });
    }

    // 1. no service installed
    void noServiceInstalled() {
        SI_CHECK(riRetrieveVariable("P") == nullptr);
        SI_CHECK(riRetrieveVariable("Cs") == nullptr);

        const fs::path withDirectory = g_scratch / "none" / "present.rib";
        touch(withDirectory);
        char found[OS_MAX_PATH_LENGTH];
        found[0] = '\0';
        SI_CHECK(riLocateFile(found, withDirectory.c_str(), nullptr, TRUE) == TRUE);
        SI_CHECK(std::string(found) == withDirectory.string());
        SI_CHECK(riLocateFile(found, (g_scratch / "none" / "absent.rib").c_str(), nullptr, TRUE) == FALSE);

        // a bare name that exists in the working directory is not found: there is no search
        touch(g_scratch / "none" / "bare.rib");
        SI_CHECK(chdir((g_scratch / "none").c_str()) == 0);
        SI_CHECK(riLocateFile(found, "bare.rib", nullptr, TRUE) == FALSE);
    }

    // 2. a lexer ReadArchive lookup with no active context
    void archiveLookupWithoutContext() {
        SI_CHECK_MSG(renderMan == nullptr, "an earlier case left an active context");
        const fs::path dir = g_scratch / "noctx";
        touch(dir / "archives" / "only_on_a_search_path.rib");
        sitest::writeFile((dir / "input.rib").string(), "ReadArchive \"only_on_a_search_path.rib\"\n");
        SI_CHECK(chdir(dir.c_str()) == 0);

        sitest::CRecordingSink sink;
        CDiagnosticSink *saved = installDiagnosticSink(&sink);
        {
            CRiStandardSession session;
            globalArena.init();
            ribParse("input.rib", nullptr); // must not crash
            globalArena.tini();
        }
        installDiagnosticSink(saved);

        bool refused = false, resolved = false;
        for (const std::string &l : sink.lines) {
            if (l.find("Failed to find") != std::string::npos)
                refused = true;
            if (l.find("only_on_a_search_path.rib\" -> \"") != std::string::npos)
                resolved = true;
        }
        SI_CHECK_MSG(refused, "the lookup with no active context did not report \"Failed to find\"; got:\n%s", sink.text().c_str());
        SI_CHECK_MSG(!resolved, "the lookup with no active context found a file through a search path");
    }

    struct SearchCase {
            const char *label;
            const char *name;
            bool useSearchPath;
            int tryNormalize;
    };

    // 3b. the standard search against CRenderer::locateFile, same results and same info lines
    void standardSearchEqualsCore() {
        const fs::path dir = g_scratch / "search";
        touch(dir / "on_path" / "via_path.rib");
        touch(dir / "cwd_only.rib");
        touch(dir / "sub" / "as_given.rib");
        touch(dir / "a_b.rib"); // found by the normalized retry of "a/b.rib"
        SI_CHECK(chdir(dir.c_str()) == 0);

        std::string pathDirectory = (dir / "on_path").string() + "/";
        TSearchpath onePath;
        onePath.directory = const_cast<char *>(pathDirectory.c_str());
        onePath.next = nullptr;

        const SearchCase cases[] = {
            {"given with a separator, exists", "sub/as_given.rib", false, TRUE},
            {"given with a separator, missing", "sub/missing.rib", false, TRUE},
            {"bare, on the search path", "via_path.rib", true, TRUE},
            {"bare, only in the working directory", "cwd_only.rib", true, TRUE},
            {"bare, only in the working directory, no search path", "cwd_only.rib", false, TRUE},
            {"normalized retry", "a/b.rib", false, TRUE},
            {"normalized retry off", "a/b.rib", false, FALSE},
            {"missing bare", "nowhere.rib", true, TRUE},
        };

        CRiStandardSession session;
        for (const SearchCase &c : cases) {
            TSearchpath *sp = c.useSearchPath ? &onePath : nullptr;
            char coreResult[OS_MAX_PATH_LENGTH], sceneResult[OS_MAX_PATH_LENGTH];
            coreResult[0] = sceneResult[0] = '\0';

            sitest::CRecordingSink coreSink, sceneSink;
            CDiagnosticSink *saved = installDiagnosticSink(&coreSink);
            const int core = CRenderer::locateFile(coreResult, c.name, sp, c.tryNormalize);
            installDiagnosticSink(&sceneSink);
            const int scene = riLocateFile(sceneResult, c.name, sp, c.tryNormalize);
            installDiagnosticSink(saved);

            SI_CHECK_MSG(core == scene, "%s: CRenderer::locateFile says %d, riLocateFile says %d", c.label, core, scene);
            if (core == TRUE && scene == TRUE)
                SI_CHECK_MSG(std::string(coreResult) == sceneResult, "%s: found \"%s\" against \"%s\"", c.label, coreResult, sceneResult);
            SI_CHECK_MSG(coreSink.lines == sceneSink.lines, "%s: the info lines differ:\ncore:\n%sscene:\n%s", c.label, coreSink.text().c_str(),
                         sceneSink.text().c_str());
        }
    }

    // 3. a standard session
    void standardSession() {
        {
            CRiStandardSession session;
            SI_CHECK(CDeclarations::active());
            SI_CHECK(riRetrieveVariable("P") != nullptr);
            SI_CHECK(riRetrieveVariable("not_declared_anywhere") == nullptr);
            const std::string table = declaredSetOfTable();
            SI_CHECK_MSG(table == g_declaredSetReference, "the standard session's declared set differs from reference/declared_set.txt: %s",
                         sitest::firstDifference(g_declaredSetReference, table).c_str());
        }
        SI_CHECK_MSG(!CDeclarations::active(), "the table outlived the session");
        standardSearchEqualsCore();
    }

    struct CMarkerDeclarations : CRiDeclarationService {
            CVariable *retrieveVariable(const char *) override { return nullptr; }
            CVariable *declareVariable(const char *, const char *) override { return nullptr; }
    };
    struct CMarkerSearch : CRiFileSearchService {
            int locateFile(char *, const char *, TSearchpath *, int) override { return FALSE; }
    };

    // 4. nested sessions
    void nestedSessions() {
        CMarkerDeclarations markerDeclarations;
        CMarkerSearch markerSearch;
        CRiDeclarationService *previousDeclarations = riInstallDeclarationService(&markerDeclarations);
        CRiFileSearchService *previousSearch = riInstallFileSearchService(&markerSearch);
        SI_CHECK(riRetrieveVariable("P") == nullptr);

        {
            CRiStandardSession outer;
            SI_CHECK(riRetrieveVariable("P") != nullptr);
            {
                CRiStandardSession inner;
                SI_CHECK(riRetrieveVariable("P") != nullptr);
            }
            SI_CHECK_MSG(riRetrieveVariable("P") != nullptr && CDeclarations::active(), "the inner session's end removed the outer session's services");
        }

        // the services that were installed by hand are back, and the table is gone
        SI_CHECK(riRetrieveVariable("P") == nullptr);
        SI_CHECK_MSG(riInstallDeclarationService(previousDeclarations) == &markerDeclarations, "the hand-installed declaration service was not restored");
        SI_CHECK_MSG(riInstallFileSearchService(previousSearch) == &markerSearch, "the hand-installed file search service was not restored");
        SI_CHECK(!CDeclarations::active());
    }

    // 5. riEvaluateCondition with null accessors
    void conditionWithNullAccessors() {
        CRiInterface context; // the base class answers nullptr from getAttributes() and getOptions()
        SI_CHECK(context.getAttributes(TRUE) == nullptr);
        SI_CHECK(context.getOptions() == nullptr);

        sitest::CRecordingSink sink;
        CDiagnosticSink *saved = installDiagnosticSink(&sink);
        globalArena.init();
        const int userResult = riEvaluateCondition(&context, "$user:level == 1");
        const int optionResult = riEvaluateCondition(&context, "$Option:limits:bucketsize == 16");
        globalArena.tini();
        installDiagnosticSink(saved);

        SI_CHECK_MSG(userResult == FALSE, "a variable looked up through null accessors evaluated to %d, expected FALSE (not found)", userResult);
        SI_CHECK_MSG(optionResult == FALSE, "an option looked up through null accessors evaluated to %d, expected FALSE (not found)", optionResult);
        int notFound = 0;
        for (const std::string &l : sink.lines)
            if (l.find("Failed to find variable") != std::string::npos)
                ++notFound;
        SI_CHECK_MSG(notFound == 2, "expected two \"Failed to find variable\" reports, got %d:\n%s", notFound, sink.text().c_str());
    }

    // 6. a renderer session's declared set
    void rendererSession() {
        RiBegin(RI_NULL);
        SI_CHECK(CDeclarations::active());
        const std::string table = declaredSetOfTable();
        SI_CHECK_MSG(table == g_declaredSetReference, "the renderer session's declared set differs from reference/declared_set.txt: %s",
                     sitest::firstDifference(g_declaredSetReference, table).c_str());
        SI_CHECK(CRenderer::retrieveVariable("P") == CDeclarations::find("P"));
        RiEnd();
        SI_CHECK(!CDeclarations::active());
    }

    // 7. D4-a: after a renderer session a bare name in the working directory is found by the standard search
    void bareNameAfterRendererSession() {
        const fs::path dir = g_scratch / "after_renderer";
        touch(dir / "only_here.rib");
        SI_CHECK(chdir(dir.c_str()) == 0);

        CRiStandardSession session;
        sitest::CRecordingSink sink;
        CDiagnosticSink *saved = installDiagnosticSink(&sink);
        char found[OS_MAX_PATH_LENGTH];
        found[0] = '\0';
        const int result = riLocateFile(found, "only_here.rib", nullptr, TRUE);
        installDiagnosticSink(saved);

        SI_CHECK_MSG(result == TRUE, "a bare name that exists only in the working directory was not found after a renderer session");
        SI_CHECK_MSG(std::string(found) == "only_here.rib", "found \"%s\", expected \"only_here.rib\"", found);
        bool line = false;
        for (const std::string &l : sink.lines)
            if (l.find("\"only_here.rib\" -> \"only_here.rib\"") != std::string::npos)
                line = true;
        SI_CHECK_MSG(line, "missing the info line \"only_here.rib\" -> \"only_here.rib\"; got:\n%s", sink.text().c_str());
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <reference dir> <scratch dir>\n", argv[0]);
        return 2;
    }
    g_scratch = fs::absolute(argv[2]);
    std::error_code ec;
    fs::remove_all(g_scratch, ec);
    fs::create_directories(g_scratch);
    g_declaredSetReference = sitest::readFile(sitest::RefLayout{argv[1], ""}.lookup("declared_set.txt"));
    SI_CHECK_MSG(!g_declaredSetReference.empty(), "missing reference/declared_set.txt (build the target scene-interface-capture-baselines)");

    noServiceInstalled();
    archiveLookupWithoutContext();
    standardSession(); // includes the comparison with CRenderer::locateFile: before any renderer session
    nestedSessions();
    conditionWithNullAccessors();
    rendererSession();
    bareNameAfterRendererSession();

    if (sitest::failures() != 0) {
        std::fprintf(stderr, "SceneInterface_Services: %d failure(s)\n", sitest::failures());
        return 1;
    }
    std::printf("SceneInterface_Services: ok\n");
    return 0;
}

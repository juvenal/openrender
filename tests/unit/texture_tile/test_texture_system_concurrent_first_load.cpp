/**
 * tests/unit/texture_tile/test_texture_system_concurrent_first_load.cpp
 *
 * TextureSystem_ConcurrentFirstLoad and TextureSystem_ConcurrentFirstLoadExternalMutex
 * (spec 027, T-C1, T022 and T022a, FR-016, FR-018(c), GitHub #20, Edge Cases "first-use races").
 *
 * Four threads behind a start barrier ask one fresh texture system for the same names:
 * an existing baked texture and a missing one. Every repeat builds a fresh system with a counting
 * resolver that sleeps 1 ms inside locate() to hold the race window open, and asserts
 *   - the four handles for one name are equal and non-null (a missing name yields a substitute);
 *   - each name is located exactly once;
 *   - the missing name is reported exactly once (one CODE_NOFILE).
 * 200 repeats. A plain build, no ThreadSanitizer (that stays with #19).
 *
 * Argument "external-mutex": the system is created with a caller-supplied plain, non-recursive
 * mutex as CTextureSystemSettings::loadMutex (the production configuration: the core passes
 * &CRenderer::shaderMutex). Same assertions, plus the external mutex is free (a try-lock
 * succeeds) after every handle() returns and, in builds where NDEBUG is not defined, a handle()
 * made by a thread that is already inside handle() trips the re-entry assertion (a child
 * process, since the assertion aborts). Reported as skipped in Release.
 *
 * Red: the test cannot be built until textureSystem.h exists (T033); at Phase 5's adapter step it
 * is red by assertion (more than one locate, unequal handles), because the moved find-load-insert
 * sequence has no lock yet (T064). Links ribRender like TileSource_TiffInfo.
 */

#include "texture_system_test_doubles.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

static const int kThreads = 4;
static const int kRepeats = 200;
static const char *kExisting = "concurrency_rgb.tex";
static const char *kMissing = "nosuchfile_027_concurrent.tex";

static std::string fixturesDir() {
    const char *dir = getenv("TEXTURE_TILE_FIXTURES_DIR");
    return dir ? dir : "tests/unit/texture_tile/fixtures";
}

// One repeat: a fresh system, kThreads threads released together.
static int oneRepeat(CCountingDiagnostics &diagnostics, TMutex *externalMutex) {
    CDirectoryResolver directories({fixturesDir()});
    CCountingResolver counting(&directories);
    diagnostics.reset();

    CTextureSystem *system = createTextureSystem(testSystemSettings(&counting, kThreads, externalMutex));
    CHECK(system != nullptr);

    TextureHandle *existing[kThreads] = {};
    TextureHandle *missing[kThreads] = {};
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; i++) {
        threads.emplace_back([&, i]() {
            ++ready;
            while (!go.load())
                std::this_thread::yield();
            existing[i] = system->handle(kExisting, ETextureKind::Texture);
            missing[i] = system->handle(kMissing, ETextureKind::Texture);
        });
    }
    while (ready.load() < kThreads)
        std::this_thread::yield();
    go = true;
    for (std::thread &t : threads)
        t.join();

    for (int i = 0; i < kThreads; i++) {
        CHECK(existing[i] != nullptr);
        CHECK(missing[i] != nullptr); // a substitute, never nullptr
        CHECK(existing[i] == existing[0]);
        CHECK(missing[i] == missing[0]);
    }
    CHECK(existing[0] != missing[0]);
    CHECK(counting.count(kExisting) == 1);
    CHECK(counting.count(kMissing) == 1);
    CHECK(diagnostics.noFile() == 1);

    if (externalMutex) {
        // handle() must have released the caller's mutex on every path.
        CHECK(pthread_mutex_trylock(externalMutex) == 0);
        pthread_mutex_unlock(externalMutex);
    }

    delete system;
    return 0;
}

#ifndef NDEBUG
// A resolver whose locate() calls handle() again: a re-entry on the same thread.
class CReentrantResolver : public CTextureFileResolver {
    public:
        CTextureSystem *system = nullptr;
        int locate(char *, const char *name) const override {
            if (strcmp(name, "reentry_inner.tex") != 0)
                system->handle("reentry_inner.tex", ETextureKind::Texture);
            return FALSE;
        }
};

// The re-entry assertion of T071a, in a child process because it aborts.
static int reentryDeathSubtest() {
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        alarm(20); // a missing assertion would self-deadlock on the plain mutex; end the child with SIGALRM
        TMutex mutex;
        osCreateMutex(mutex);
        CReentrantResolver resolver;
        CTextureSystem *system = createTextureSystem(testSystemSettings(&resolver, 1, &mutex));
        if (!system)
            _exit(3);
        resolver.system = system;
        system->handle("reentry_outer.tex", ETextureKind::Texture);
        _exit(0); // returned normally: the assertion did not fire
    }
    int status = 0;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFSIGNALED(status));
    CHECK(WTERMSIG(status) == SIGABRT);
    printf("re-entry assertion fired (child ended with SIGABRT)\n");
    return 0;
}
#endif

int main(int argc, char **argv) {
    const bool external = (argc > 1 && strcmp(argv[1], "external-mutex") == 0);

    CCountingDiagnostics diagnostics;
    CDiagnosticSink *previous = installDiagnosticSink(&diagnostics);

    TMutex mutex;
    if (external) {
        osCreateMutex(mutex);
#ifndef NDEBUG
        if (reentryDeathSubtest() != 0)
            return 1;
#else
        printf("re-entry death subtest: skipped (NDEBUG)\n");
#endif
    }

    for (int repeat = 0; repeat < kRepeats; repeat++) {
        if (oneRepeat(diagnostics, external ? &mutex : nullptr) != 0) {
            fprintf(stderr, "failed in repeat %d\n", repeat);
            installDiagnosticSink(previous);
            return 1;
        }
    }

    if (external)
        osDeleteMutex(mutex);
    installDiagnosticSink(previous);
    printf("%s: %d repeats, %d threads: one handle, one locate, one CODE_NOFILE each\n",
           external ? "external-mutex" : "internal-mutex", kRepeats, kThreads);
    return 0;
}

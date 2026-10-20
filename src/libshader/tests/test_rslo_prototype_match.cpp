/**
 * tests/test_rslo_prototype_match.cpp
 *
 * GitHub #67 regression: setOpcode() in rslo.y, the .rslo loader's binding
 * of a shader's function call to a built-in function, read past the
 * terminator of a prototype. The matcher now lives in
 * src/libshader/shading/rsloPrototypeMatch.h as a pure function, so it can
 * be driven directly here.
 *
 * Two kinds of check:
 *
 *   - Semantics: representative table prototypes from scriptFunctions.h /
 *     shaderFunctions.h against call prototypes, pinning what '+', '*', '.',
 *     '!' and the exact type letters match.
 *
 *   - Overreads: each string is copied so that its '\0' is the last
 *     readable byte before a PROT_NONE page. A read past either terminator
 *     faults. Every such case runs in its own forked child, so a fault is
 *     reported per case instead of ending the run. Control cases use the
 *     same layout and must not fault.
 *
 * The trigger found by ASan (Visual_sphere-shadername-reyes) is
 * shadername("surface"): the call prototype "s=S" is tried against the
 * zero-argument entry "s=" first; that table prototype ends while the call
 * does not, and the matcher then read "s="[3].
 */

#include <csignal>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "rsloPrototypeMatch.h"

static int g_passed = 0;
static int g_failed = 0;

static void check(bool ok, const char *what, const char *table, const char *call) {
    if (ok) {
        ++g_passed;
    }
    else {
        fprintf(stderr, "FAIL: %s  table=\"%s\" call=\"%s\"\n", what, table, call);
        ++g_failed;
    }
}

// --------------------------------------------------------------------------
// Semantics
// --------------------------------------------------------------------------

struct Case {
        const char *table;
        const char *call;
        bool match;
};

static const Case kSemantics[] = {
    // Exact
    {"s=S", "s=S", true},
    {"s=", "s=", true},
    {"o=Mfff", "o=Mfff", true},
    // Return types must be identical
    {"f=f", "c=f", false},
    {"f=f+", "s=f", false},
    // 's' and 'S' (and every other letter pair) are distinct
    {"s=S", "s=s", false},
    {"s=s", "s=S", false},
    {"f=fv", "f=ff", false},
    // Call longer than the table: no match (the GitHub #67 shape)
    {"s=", "s=S", false},
    {"f=f", "f=ff", false},
    {"f=s.", "f=sff", false},
    // Call shorter than the table, no wildcard: no match
    {"f=ff", "f=f", false},
    {"s=", "s=", true},
    // '.' matches any one parameter
    {"f=s.", "f=sf", true},
    {"f=s.", "f=sv", true},
    {"f=s.", "f=s", false},
    // ".*": zero or more of anything
    {"s=s.*", "s=s", true},
    {"s=s.*", "s=sf", true},
    {"s=s.*", "s=sfvS", true},
    {"s=s.*", "s=", false},
    {"s=s.*", "s=f", false},
    {"o=s.*", "o=sfff", true},
    // "x*": zero or more x, and nothing after them
    {"f=fffff*", "f=ffff", true},
    {"f=fffff*", "f=fffff", true},
    {"f=fffff*", "f=fffffff", true},
    {"f=fffff*", "f=fff", false},
    {"f=fffff*", "f=ffffv", false},
    {"f=fffff*", "f=fffffv", false},
    {"c=Sfcccc*", "c=Sfccc", true},
    {"c=Sfcccc*", "c=Sfcccccc", true},
    {"c=Sfcccc*", "c=Sfcc", false},
    {"v=Sfvvvv*", "v=sfvvv", false},
    // "x+": one or more x
    {"f=f+", "f=f", true},
    {"f=f+", "f=fff", true},
    {"f=f+", "f=", false},
    {"f=f+", "f=v", false},
    {"f=f+", "f=fv", false},
    {"v=v+", "v=vv", true},
    // '!': zero or more (name, value) pairs, the name 's' or 'S'
    {"f=ff!", "f=ff", true},
    {"f=ff!", "f=ffSf", true},
    {"f=ff!", "f=ffsv", true},
    {"f=ff!", "f=ffSfSc", true},
    {"f=ff!", "f=ffSfsS", true},
    {"f=ff!", "f=fff", false},
    {"f=ff!", "f=ffSff", false},
    {"f=ff!", "f=f", false},
    {"o=spnff!", "o=spnff", true},
    {"o=spnff!", "o=spnffSf", true},
    {"f=SSpn!", "f=SSpnSfSv", true},
    {"f=SSpn!", "f=sSpn", false},
};

static void runSemantics() {
    for (const Case &c : kSemantics) {
        bool got = libshader::rsloPrototypeMatches(c.table, c.call);
        check(got == c.match, c.match ? "expected match" : "expected no match", c.table, c.call);
    }
}

// --------------------------------------------------------------------------
// Overreads: guard-paged copies, one forked child per case
// --------------------------------------------------------------------------

// Copy s so that its terminator is the last readable byte before a PROT_NONE
// page. Never unmapped: the child exits right after using it.
static const char *guarded(const char *s) {
    const size_t page = (size_t)sysconf(_SC_PAGESIZE);
    const size_t len = strlen(s) + 1;
    void *mem = mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        _exit(90);
    }
    char *base = (char *)mem;
    if (mprotect(base + page, page, PROT_NONE) != 0) {
        perror("mprotect");
        _exit(91);
    }
    char *dst = base + page - len;
    memcpy(dst, s, len);
    return dst;
}

enum { kChildMatch = 10,
       kChildNoMatch = 11,
       kChildDisagree = 12 };

// Runs the matcher on guarded copies of both strings in a child. Returns
// the child's exit code, or -signal if it was killed.
static int runGuardedChild(const char *table, const char *call) {
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1000;
    }
    if (pid == 0) {
        const char *t = guarded(table);
        const char *c = guarded(call);
        bool got = libshader::rsloPrototypeMatches(t, c);
        // The guarded answer must equal the answer on ordinary copies:
        // the layout of the bytes after a terminator must never matter.
        std::string ts(table), cs(call);
        bool plain = libshader::rsloPrototypeMatches(ts.c_str(), cs.c_str());
        if (got != plain)
            _exit(kChildDisagree);
        _exit(got ? kChildMatch : kChildNoMatch);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) != pid) {
        perror("waitpid");
        return -1001;
    }
    if (WIFSIGNALED(status))
        return -WTERMSIG(status);
    return WEXITSTATUS(status);
}

static void guardCase(const char *label, const char *table, const char *call, bool expectMatch) {
    int rc = runGuardedChild(table, call);
    if (rc < 0 && rc > -1000) {
        int sig = -rc;
        fprintf(stderr, "FAIL: [%s] read past a terminator: child killed by %s (signal %d)  table=\"%s\" call=\"%s\"\n",
                label, sig == SIGSEGV ? "SIGSEGV" : sig == SIGBUS ? "SIGBUS"
                                                                  : "a signal",
                sig, table, call);
        ++g_failed;
        return;
    }
    if (rc == kChildDisagree) {
        fprintf(stderr, "FAIL: [%s] guarded and plain copies disagree  table=\"%s\" call=\"%s\"\n", label, table,
                call);
        ++g_failed;
        return;
    }
    if (rc != kChildMatch && rc != kChildNoMatch) {
        fprintf(stderr, "FAIL: [%s] child harness error %d\n", label, rc);
        ++g_failed;
        return;
    }
    check((rc == kChildMatch) == expectMatch, label, table, call);
}

static void runGuarded() {
    // Controls: the same layout, no read past a terminator. These must pass
    // before and after the fix; they show a fault means an overread.
    guardCase("control: exact", "s=S", "s=S", true);
    guardCase("control: zero-arg exact", "s=", "s=", true);
    guardCase("control: star, call ends early", "f=fffff*", "f=ffff", true);
    guardCase("control: param list pair", "f=ff!", "f=ffSf", true);
    guardCase("control: call ends before table", "f=ff", "f=f", false);

    // GitHub #67: the table prototype ends while the call's does not.
    guardCase("#67 shadername(\"surface\") vs \"s=\"", "s=", "s=S", false);
    guardCase("#67 table ends first", "f=f", "f=ff", false);
    guardCase("#67 table ends first after '.'", "f=s.", "f=sff", false);

    // Call side: a parameter list ending in a name with no value. The
    // matcher stepped over the name and its (missing) value, past the
    // call's terminator. Binding it like the compiler does (rslo.cpp's
    // matcher accepts it) is the proposed semantic.
    guardCase("call-side '!' dangling name", "f=ff!", "f=ffS", true);
    guardCase("call-side '!' dangling name after a pair", "f=ff!", "f=ffSfs", true);

    // Sweep: every table prototype against every call prototype used above,
    // all guarded. Catches any other read past either terminator.
    std::set<std::string> tables, calls;
    for (const Case &c : kSemantics) {
        tables.insert(c.table);
        calls.insert(c.call);
    }
    for (const std::string &table : tables) {
        for (const std::string &call : calls) {
            int rc = runGuardedChild(table.c_str(), call.c_str());
            if (rc < 0 || rc == kChildDisagree || (rc != kChildMatch && rc != kChildNoMatch)) {
                fprintf(stderr, "FAIL: [sweep] %s  table=\"%s\" call=\"%s\"\n",
                        rc < 0 ? "read past a terminator (child killed by a signal)"
                               : "guarded and plain copies disagree or harness error",
                        table.c_str(), call.c_str());
                ++g_failed;
            }
            else {
                ++g_passed;
            }
        }
    }
}

int main() {
    runSemantics();
    runGuarded();

    printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}

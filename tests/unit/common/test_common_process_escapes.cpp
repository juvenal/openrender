// GitHub #65: osProcessEscapes() (src/common/os.cpp) must never touch a byte
// past the string's terminator, and must decode every escape in the string.
//
// Each case runs in a forked child. The input is copied so that its
// terminator is the last byte of a readable page followed by a PROT_NONE
// guard page, so any read or write past the terminator faults
// deterministically (no ASan needed). The parent reports a fault, or an
// unexpected result, as a failure.

#include <cstdio>
#include <cstring>
#include <string>

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common/os.h"

namespace {

struct Case {
        const char *name;
        const char *input;
        const char *expected;
};

// Escaped for C: "a\\n" is the three characters a, backslash, n.
const Case kCases[] = {
    {"trailing_newline", "abc\\n", "abc\n"},
    {"trailing_backslash", "abc\\\\", "abc\\"},
    {"only_escape", "\\t", "\t"},
    {"middle_escape", "a\\tb", "a\tb"},
    {"consecutive_escapes", "\\n\\t", "\n\t"},
    {"consecutive_backslash_then_newline", "x\\\\\\ny", "x\\\ny"},
    {"escaped_backslash_then_letter", "\\\\n", "\\n"},
    {"trailing_lone_backslash", "abc\\", "abc\\"},
    // An unknown escape is kept as written, backslash and character both
    // (before #65 the character after the backslash was dropped).
    {"unknown_escape_middle", "a\\qb", "a\\qb"},
    {"unknown_escape_trailing", "ab\\q", "ab\\q"},
    {"empty", "", ""},
    {"no_escapes", "plain text", "plain text"},
};

std::string visible(const char *s) {
    std::string out;
    for (; *s != '\0'; s++) {
        switch (*s) {
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            case '\\': out += "\\\\"; break;
            default: out += *s; break;
        }
    }
    return out;
}

// Runs in the child: 0 on success, 1 on a wrong result, 2 on a setup error.
int runCase(const Case &c) {
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    char *base = static_cast<char *>(
        mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));
    if (base == MAP_FAILED) return 2;
    if (mprotect(base + page, page, PROT_NONE) != 0) return 2;

    const size_t len = strlen(c.input);
    char *str = base + page - (len + 1); // terminator is the page's last byte
    memcpy(str, c.input, len + 1);

    osProcessEscapes(str);

    if (strcmp(str, c.expected) != 0) {
        fprintf(stderr, "FAIL %s: input \"%s\": got \"%s\", expected \"%s\"\n", c.name,
                visible(c.input).c_str(), visible(str).c_str(), visible(c.expected).c_str());
        return 1;
    }
    return 0;
}

} // namespace

int main() {
    int failures = 0;
    for (const Case &c : kCases) {
        fflush(stdout);
        fflush(stderr);
        const pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 2;
        }
        if (pid == 0) _exit(runCase(c));

        int status = 0;
        if (waitpid(pid, &status, 0) != pid) {
            perror("waitpid");
            return 2;
        }
        if (WIFSIGNALED(status)) {
            fprintf(stderr, "FAIL %s: input \"%s\": killed by signal %d (%s) -- access past the terminator\n",
                    c.name, visible(c.input).c_str(), WTERMSIG(status), strsignal(WTERMSIG(status)));
            failures++;
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 2)
                fprintf(stderr, "FAIL %s: guard-page setup failed\n", c.name);
            failures++;
        } else {
            printf("ok   %s\n", c.name);
        }
    }
    if (failures != 0) {
        fprintf(stderr, "%d case(s) failed\n", failures);
        return 1;
    }
    printf("all %zu cases passed\n", sizeof(kCases) / sizeof(kCases[0]));
    return 0;
}

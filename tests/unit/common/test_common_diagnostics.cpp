// Spec 023 (FR-002, FR-015, SC-003): diagnostics with no RI context active.
//
// No RiBegin is ever called here, so no RI context exists. Each mode reports
// one diagnostic carrying a unique marker, then prints CONTINUED to stdout and
// returns 0. diagnostics_runner.cmake checks which stream the marker reached
// and how the process ended. The default sink must apply RiErrorPrint's rules:
//   error   -> stderr, execution continues
//   warning -> stdout
//   info    -> stdout only at INFO verbosity or more (info_quiet: WARN level)
//   fatal   -> stderr, then exit(-1) (status 255), never a crash
//
// Usage: test_common_diagnostics <error|warning|info|info_quiet|fatal>

#include <cstdio>
#include <cstring>

#include "common/diagnostics.h"
#include "includes/logging.hpp"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <error|warning|info|info_quiet|fatal>\n", argv[0]);
        return 2;
    }
    const char *mode = argv[1];

    if (strcmp(mode, "error") == 0) {
        error(CODE_BADTOKEN, "MARKER-error-no-context\n");
    }
    else if (strcmp(mode, "warning") == 0) {
        warning(CODE_BADTOKEN, "MARKER-warning-no-context\n");
    }
    else if (strcmp(mode, "info") == 0) {
        set_log_level(LogLevel::INFO);
        info(CODE_BADTOKEN, "MARKER-info-no-context\n");
    }
    else if (strcmp(mode, "info_quiet") == 0) {
        set_log_level(LogLevel::WARN);
        info(CODE_BADTOKEN, "MARKER-info_quiet-no-context\n");
    }
    else if (strcmp(mode, "fatal") == 0) {
        fatal(CODE_BADTOKEN, "MARKER-fatal-no-context\n");
    }
    else {
        fprintf(stderr, "unknown mode %s\n", mode);
        return 2;
    }

    fflush(stderr);
    printf("CONTINUED\n");
    fflush(stdout);
    return 0;
}

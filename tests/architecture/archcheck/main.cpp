/**
 * Project: openRender
 *
 * File: main.cpp
 *
 * Description:
 *   archcheck: checks the build tree against constitution Principle VIII's
 *   allowed-dependency table (spec 022). Command line per
 *   specs/022-domain-dependency-guardrails/contracts/archcheck-cli.md.
 *
 *   Setup stub: parses the contract options and exits 2 ("not implemented"),
 *   so every fixture fails until the check exists (TDD red).
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
#include <string_view>

namespace {

constexpr int kExitPass = 0;
constexpr int kExitError = 2;

struct Options {
    std::string sourceDir;
    std::string buildDir;
    std::string configDir;
    std::string registry;
    std::string platform;
    std::string format = "text";
    bool emitAllowlist = false;
    bool quiet = false;
};

void printUsage(FILE *out) {
    std::fputs("usage: archcheck --source-dir <repo> --build-dir <build> --config <dir>\n"
               "                 [--platform macos|linux] [--format text|json]\n"
               "                 [--emit-allowlist] [--registry <file>] [--quiet]\n"
               "       archcheck --help\n"
               "\n"
               "Checks a CMake build tree against the Principle VIII allowed-dependency\n"
               "table (include edges from compiler depfiles, link edges from nm).\n"
               "\n"
               "  --source-dir      repository root; only <source-dir>/src/ is scanned\n"
               "  --build-dir       CMake build tree holding objects and depfiles\n"
               "  --config          directory with domain-map.txt, domain-rules.txt,\n"
               "                    allowlist.txt and metrics-baseline.txt\n"
               "  --registry        registry markdown file\n"
               "                    (default <source-dir>/DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md)\n"
               "  --platform        allowlist platform tag to apply (default: host)\n"
               "  --format          text (default) or json\n"
               "  --emit-allowlist  print every current non-allowed edge as allowlist lines\n"
               "  --quiet           text mode: only failures, warnings and the summary\n"
               "\n"
               "Exit: 0 pass, 1 check failed, 2 configuration or environment error.\n",
               out);
}

// Returns kExitPass to continue, or the exit code to stop with.
int parseArgs(int argc, char **argv, Options &opt, bool &helpOnly) {
    for (int i = 1; i < argc; i++) {
        const std::string_view arg = argv[i];
        auto value = [&](std::string &dst) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "archcheck: %s expects a value\n", argv[i]);
                return false;
            }
            dst = argv[++i];
            return true;
        };

        if (arg == "--help" || arg == "-h") {
            helpOnly = true;
            return kExitPass;
        }
        else if (arg == "--source-dir") {
            if (!value(opt.sourceDir))
                return kExitError;
        }
        else if (arg == "--build-dir") {
            if (!value(opt.buildDir))
                return kExitError;
        }
        else if (arg == "--config") {
            if (!value(opt.configDir))
                return kExitError;
        }
        else if (arg == "--registry") {
            if (!value(opt.registry))
                return kExitError;
        }
        else if (arg == "--platform") {
            if (!value(opt.platform))
                return kExitError;
            if (opt.platform != "macos" && opt.platform != "linux") {
                std::fprintf(stderr, "archcheck: --platform must be macos or linux\n");
                return kExitError;
            }
        }
        else if (arg == "--format") {
            if (!value(opt.format))
                return kExitError;
            if (opt.format != "text" && opt.format != "json") {
                std::fprintf(stderr, "archcheck: --format must be text or json\n");
                return kExitError;
            }
        }
        else if (arg == "--emit-allowlist") {
            opt.emitAllowlist = true;
        }
        else if (arg == "--quiet") {
            opt.quiet = true;
        }
        else {
            std::fprintf(stderr, "archcheck: unknown option %s\n", argv[i]);
            printUsage(stderr);
            return kExitError;
        }
    }

    if (opt.sourceDir.empty() || opt.buildDir.empty() || opt.configDir.empty()) {
        std::fprintf(stderr, "archcheck: --source-dir, --build-dir and --config are required\n");
        printUsage(stderr);
        return kExitError;
    }
    return kExitPass;
}

} // namespace

int main(int argc, char **argv) {
    Options opt;
    bool helpOnly = false;
    if (const int rc = parseArgs(argc, argv, opt, helpOnly); rc != kExitPass)
        return rc;
    if (helpOnly) {
        printUsage(stdout);
        return kExitPass;
    }

    std::fprintf(stderr, "archcheck: not implemented\n");
    return kExitError;
}

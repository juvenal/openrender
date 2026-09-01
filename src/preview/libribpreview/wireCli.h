#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIRE_CLI_EXIT = 0, // *exitCode is the process exit code the caller should use; no window
    WIRE_CLI_OPEN = 1, // proceed to the normal GUI flow, opening *outPath
} WireCliAction;

// Parses argv[1..argc-1] per contracts/cli-interface.md. --help/--version/--json are handled
// entirely here (written to stdout/stderr); *exitCode is set to one of the documented codes
// (0-4; 5 is a GUI-only path this function never reaches) whenever the return is WIRE_CLI_EXIT.
// *outPath is malloc'd (caller frees it) and set whenever the return is WIRE_CLI_OPEN. Never
// requires ORENDERHOME/SHADERS/DISPLAYS to be set.
WireCliAction wireCliRun(int argc, char **argv, char **outPath, int *exitCode);

#ifdef __cplusplus
}
#endif

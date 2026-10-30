/**
 * Project: openRender
 *
 * File: test_load_missing_module.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), PluginAbi_LoadMissingModule (maintainer's decision at
 *   checkpoint 2): osLoadModule returns NULL for a module that cannot be loaded; it never aborts.
 *
 *   The retry after a failed dlopen resolves the name with realpath into a buffer of
 *   OS_MAX_PATH_LENGTH bytes, smaller than PATH_MAX. glibc's fortified realpath aborts on that
 *   ("*** buffer overflow detected ***") before it looks at the name, so on Linux every failed
 *   first dlopen ends the process. This is what made ProceduresAbi_BareNameLocated end in
 *   "Subprocess aborted" on Linux in Phase 1.
 *
 *   argv[1]: a regular file that is not a shared object (the program passes its own source path).
 *   Cases: a bare name that exists nowhere; a relative path that does not exist; an existing file
 *   that is not a module. Each must return NULL and leave osModuleError() callable.
 *
 *   Linux: red on the starting tree (the process aborts). macOS: green (no fortified realpath),
 *   kept as a characterization. Green on both after T055a.
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

#include "common/os.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_load_missing_module <a file that is not a module>\n");
        return 2;
    }
    const char *names[3] = {"or037_no_such_module", "./or037/no/such/module.dsply", argv[1]};
    int failed = 0;
    for (const char *name : names) {
        fflush(stdout);
        void *module = osLoadModule(name);
        const char *why = osModuleError();
        if (module != nullptr) {
            printf("FAIL %s: loaded something\n", name);
            osUnloadModule(module);
            ++failed;
        }
        else {
            printf("ok   %s -> NULL (%s)\n", name, why != nullptr ? "with a reason" : "no reason");
        }
    }
    return failed == 0 ? 0 : 1;
}

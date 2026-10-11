// Spec 023 T052: dlopen each installed RI library by absolute path with no
// loader variables. Usage: dlprobe now-local|lazy-global <lib>...
// Reports where error() and parseVariable() resolve (libRIBCommon since T020/T042,
// reached through the RI library's LC_REEXPORT_DYLIB).
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    int mode = strcmp(argv[1], "now-local") == 0 ? (RTLD_NOW | RTLD_LOCAL) : (RTLD_LAZY | RTLD_GLOBAL);
    int bad = 0;
    for (int i = 2; i < argc; i++) {
        void *h = dlopen(argv[i], mode);
        if (!h) { printf("LOADFAIL %s\n    %s\n", strrchr(argv[i], '/') + 1, dlerror()); bad++; continue; }
        const char *names[] = {"_Z5error10EErrorCodePKcz", "_Z13parseVariableP9CVariablePKcS2_"};
        for (int k = 0; k < 2; k++) {
            void *s = dlsym(h, names[k]);
            Dl_info di = {0};
            if (s) dladdr(s, &di);
            printf("%-4s %-22s %-40s from %s\n", s ? "OK" : "MISS", strrchr(argv[i], '/') + 1, names[k],
                   di.dli_fname ? strrchr(di.dli_fname, '/') + 1 : "-");
            if (!s) bad++;
        }
    }
    return bad != 0;
}

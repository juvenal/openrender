#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>

#include "common/algebra.h"
#include "ri/texture/pointCloud.h"
#include "ribpreview_api.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(expr)                                                         \
    do {                                                                    \
        if (expr) {                                                         \
            ++g_pass;                                                       \
        }                                                                   \
        else {                                                              \
            ++g_fail;                                                       \
            fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
        }                                                                   \
    } while (0)

static void writeFile(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    fputs(content, f);
    fclose(f);
}

static void writePointCloudFixture(const char *path) {
    matrix from, to;
    identitym(from);
    identitym(to);
    char *names[1] = {(char *)"_radiosity"};
    char *types[1] = {(char *)"float"};
    CPointCloud *cloud = new CPointCloud(path, from, to, NULL, 1, names, types, TRUE);
    for (int i = 0; i < 4; i++) {
        float P[3] = {(float)i, (float)i, (float)i};
        float N[3] = {0, 0, 1};
        float C[1] = {0.5f};
        cloud->store(C, P, N, 0.1f);
    }
    delete cloud;
}

// Captures both stdout and stderr produced while `fn` runs, into two scratch files, and
// returns them as heap strings (caller frees). Restores the real fds afterward.
struct CapturedOutput {
        char *out;
        char *err;
};

static char *readWholeFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return strdup("");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(size + 1);
    size_t got = fread(buf, 1, size, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static CapturedOutput runCli(int argc, char **argv, int *exitCode) {
    const char *outPath = "test_wire_cli_stdout.txt";
    const char *errPath = "test_wire_cli_stderr.txt";

    fflush(stdout);
    fflush(stderr);
    int savedOut = dup(STDOUT_FILENO);
    int savedErr = dup(STDERR_FILENO);
    freopen(outPath, "w", stdout);
    freopen(errPath, "w", stderr);

    char *dataPath = NULL;
    WireCliAction action = wireCliRun(argc, argv, &dataPath, exitCode);
    if (action == WIRE_CLI_OPEN) {
        *exitCode = 0; // opening a window is out of this test's scope; treat as success
        free(dataPath);
    }

    fflush(stdout);
    fflush(stderr);
    dup2(savedOut, STDOUT_FILENO);
    dup2(savedErr, STDERR_FILENO);
    close(savedOut);
    close(savedErr);
    clearerr(stdout);
    clearerr(stderr);

    CapturedOutput result;
    result.out = readWholeFile(outPath);
    result.err = readWholeFile(errPath);
    remove(outPath);
    remove(errPath);
    return result;
}

// Extremely lightweight structural check -- balanced braces/brackets and no stray control
// characters -- rather than a full parser, since the project has no JSON dependency.
static bool looksLikeWellFormedJson(const char *s) {
    int braces = 0, brackets = 0;
    bool inString = false;
    for (const char *p = s; *p != '\0'; p++) {
        if (inString) {
            if (*p == '\\') {
                p++;
                continue;
            }
            if (*p == '"')
                inString = false;
            continue;
        }
        switch (*p) {
            case '"':
                inString = true;
                break;
            case '{':
                braces++;
                break;
            case '}':
                braces--;
                break;
            case '[':
                brackets++;
                break;
            case ']':
                brackets--;
                break;
        }
        if (braces < 0 || brackets < 0)
            return false;
    }
    return !inString && braces == 0 && brackets == 0;
}

static bool contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

// Extracts the numeric value following `"key":` in a small hand-emitted JSON document --
// sufficient for this test's own known-fixed output shape, without a real JSON parser.
static bool extractJsonNumber(const char *json, const char *key, double *out) {
    const char *p = strstr(json, key);
    if (p == NULL)
        return false;
    p = strchr(p, ':');
    if (p == NULL)
        return false;
    return sscanf(p + 1, "%lf", out) == 1;
}

int main() {
    // --help and --version: exit 0, no ORENDERHOME/SHADERS/DISPLAYS required (none set here).
    {
        char argv0[] = "orender-wire", argv1[] = "--help";
        char *argv[] = {argv0, argv1};
        int code = -1;
        CapturedOutput r = runCli(2, argv, &code);
        CHECK(code == 0);
        CHECK(contains(r.out, "Usage"));
        free(r.out);
        free(r.err);
    }
    {
        char argv0[] = "orender-wire", argv1[] = "--version";
        char *argv[] = {argv0, argv1};
        int code = -1;
        CapturedOutput r = runCli(2, argv, &code);
        CHECK(code == 0);
        CHECK(contains(r.out, "orender-wire"));
        free(r.out);
        free(r.err);
    }

    // Usage errors -> exit 1: no file, unknown option, two positional operands.
    {
        char argv0[] = "orender-wire";
        char *argv[] = {argv0};
        int code = -1;
        CapturedOutput r = runCli(1, argv, &code);
        CHECK(code == 1);
        free(r.out);
        free(r.err);
    }
    {
        char argv0[] = "orender-wire", argv1[] = "--bogus", argv2[] = "file.rib";
        char *argv[] = {argv0, argv1, argv2};
        int code = -1;
        CapturedOutput r = runCli(3, argv, &code);
        CHECK(code == 1);
        free(r.out);
        free(r.err);
    }
    {
        char argv0[] = "orender-wire", argv1[] = "a.rib", argv2[] = "b.rib";
        char *argv[] = {argv0, argv1, argv2};
        int code = -1;
        CapturedOutput r = runCli(3, argv, &code);
        CHECK(code == 1);
        free(r.out);
        free(r.err);
    }

    // File not found -> exit 2.
    {
        char argv0[] = "orender-wire", argv1[] = "--json", argv2[] = "does_not_exist.rib";
        char *argv[] = {argv0, argv1, argv2};
        int code = -1;
        CapturedOutput r = runCli(3, argv, &code);
        CHECK(code == 2);
        free(r.out);
        free(r.err);
    }

    // NOTE: exit code 3 ("RIB parse failed") is specified but not currently exercised here.
    // ribpreview_load()/ribParse() never signals a syntax-level parse failure -- the RIB grammar
    // recovers from malformed input rather than aborting, so ribpreview_load() always returns a
    // (possibly empty/garbage-camera) scene rather than NULL for content like
    // "this is not a valid RIB file {{{". emitRibJson()'s "return 3 if scene==NULL" branch is
    // therefore the contract's correct handler for a case the parser cannot currently produce,
    // not dead code to remove. See tasks.md T041 notes for the follow-up (give ribParse a real
    // failure-signaling channel) -- that is preview/RIB-parsing-spec territory, out of scope here.

    // Data file rejected -> exit 4: force --type=data on a file that is not a data file.
    {
        writeFile("test_wire_cli_notdata.rib", "##RenderMan RIB-Structure 1.1\nWorldBegin\nWorldEnd\n");
        char argv0[] = "orender-wire", argv1[] = "--json", argv2[] = "--type=data",
             argv3[] = "test_wire_cli_notdata.rib";
        char *argv[] = {argv0, argv1, argv2, argv3};
        int code = -1;
        CapturedOutput r = runCli(4, argv, &code);
        CHECK(code == 4);
        free(r.out);
        free(r.err);
        remove("test_wire_cli_notdata.rib");
    }

    // Auto-detect must not silently mis-route a recognized-but-incompatible data file to the RIB
    // path: a corrupted/version-mismatched data file has to fail with exit 4, not "succeed" as
    // an empty RIB scene with exit 0. Corrupt the on-disk VERSION_MAJOR field (4 bytes after the
    // leading magic number, per src/ri/dataLoad.cpp's sniffHeader) of a real point-cloud fixture.
    {
        const char *path = "test_wire_cli_badversion.ptc";
        writePointCloudFixture(path);
        FILE *f = fopen(path, "r+b");
        int badVersion = -1;
        fseek(f, sizeof(int), SEEK_SET);
        fwrite(&badVersion, sizeof(int), 1, f);
        fclose(f);

        char argv0[] = "orender-wire", argv1[] = "--json", argv2[] = "test_wire_cli_badversion.ptc";
        char *argv[] = {argv0, argv1, argv2};
        int code = -1;
        CapturedOutput r = runCli(3, argv, &code);
        CHECK(code == 4);
        free(r.out);
        free(r.err);
        remove(path);
    }

    // RIB --json: well-formed JSON, required keys, completes well under SC-004's 2s budget.
    {
        writeFile("test_wire_cli_scene.rib",
                  "##RenderMan RIB-Structure 1.1\n"
                  "version 3.03\n"
                  "Projection \"perspective\" \"fov\" 45\n"
                  "Translate 0 0 5\n"
                  "WorldBegin\n"
                  "  Sphere 1 -1 1 360\n"
                  "WorldEnd\n");
        char argv0[] = "orender-wire", argv1[] = "--json", argv2[] = "test_wire_cli_scene.rib";
        char *argv[] = {argv0, argv1, argv2};
        int code = -1;
        time_t t0 = time(NULL);
        CapturedOutput r = runCli(3, argv, &code);
        time_t t1 = time(NULL);
        CHECK(code == 0);
        CHECK((t1 - t0) < 2);
        CHECK(looksLikeWellFormedJson(r.out));
        CHECK(contains(r.out, "\"schemaVersion\": 1"));
        CHECK(contains(r.out, "\"documentType\": \"rib\""));
        CHECK(contains(r.out, "\"scene\""));
        CHECK(contains(r.out, "\"camera\""));
        free(r.out);
        free(r.err);
        remove("test_wire_cli_scene.rib");
    }

    // Data --json: well-formed JSON, required keys, and the synthesized camera actually
    // frames the reported bounds (FR-008) -- the eye-to-center distance implied by fov must
    // enclose the bounding radius with margin, checked via the near/far clipping relationship
    // the camera was built with (near/far bracket the whole box from the eye position).
    {
        writePointCloudFixture("test_wire_cli_cloud.ptc");
        char argv0[] = "orender-wire", argv1[] = "--json", argv2[] = "test_wire_cli_cloud.ptc";
        char *argv[] = {argv0, argv1, argv2};
        int code = -1;
        time_t t0 = time(NULL);
        CapturedOutput r = runCli(3, argv, &code);
        time_t t1 = time(NULL);
        CHECK(code == 0);
        CHECK((t1 - t0) < 2);
        CHECK(looksLikeWellFormedJson(r.out));
        CHECK(contains(r.out, "\"schemaVersion\": 1"));
        CHECK(contains(r.out, "\"documentType\": \"pointcloud\""));
        CHECK(contains(r.out, "\"data\""));
        CHECK(contains(r.out, "\"primitives\""));
        CHECK(contains(r.out, "\"channels\""));
        CHECK(contains(r.out, "_radiosity"));
        CHECK(contains(r.out, "\"camera\""));
        // FR-008: the synthesized camera must actually frame the reported bounds. The box
        // spans (0,0,0) to (3,3,3), diagonal sqrt(27) ~= 5.196 -- farPlane must clear the far
        // side of the box from the eye (nearPlane > 0, farPlane - nearPlane > diagonal).
        double nearP = 0, farP = 0;
        CHECK(extractJsonNumber(r.out, "\"nearPlane\"", &nearP));
        CHECK(extractJsonNumber(r.out, "\"farPlane\"", &farP));
        CHECK(nearP > 0.0);
        CHECK((farP - nearP) > 5.196);
        free(r.out);
        free(r.err);
        remove("test_wire_cli_cloud.ptc");
    }

    printf("test_wire_cli: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}

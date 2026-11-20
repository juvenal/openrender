/**
 * tests/shading_parity/test_uninitialized_locals.cpp
 *
 * GitHub #191: CProgrammableShaderInstance::prepare() took a shader's locals
 * and parameter slots from the shading arena (ralloc) and never cleared
 * them. A local read before it is assigned therefore held whatever an
 * earlier shading pass had left at that offset: a float read as a wrong
 * number, and a string was NULL or a stale pointer that the builtins
 * dereference (`==` is a strcmp, printf("%s"), and the "label" parameter of
 * trace() is handed out later by raylabel()).
 *
 * The fixture is one shader, shaded twice on one render thread so that the
 * second shading reuses the arena the first one dirtied (same layout, same
 * offsets): the first instance (init = 1) assigns the locals to recognisable
 * values, the second (init = 0) reads them without assigning. Both formats
 * share prepare(), so both are rendered. The expected value of an unassigned
 * local is 0 for a float and "" for a string; a crash is a failure too.
 *
 * A third shader prints raylabel() at the hit of the trace() the fixture
 * makes with the unassigned string as its label.
 *
 * Not covered, deliberately: a string PARAMETER with a non-constant default
 * (`string s = concat("a","b")`) reads NULL under .slo today because the
 * JIT does not run the default's init code. Defining unset string slots as ""
 * (the fix for #191) masks that as "", so do not read this test as a check
 * of that default; it is filed separately.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(expr)                                                      \
    do {                                                                       \
        if (expr) {                                                            \
            ++g_passed;                                                        \
        }                                                                      \
        else {                                                                 \
            fprintf(stderr, "FAIL: %s  (%s:%d)\n", #expr, __FILE__, __LINE__); \
            ++g_failed;                                                        \
        }                                                                      \
    } while (0)

static bool writeFile(const std::string &path, const std::string &content) {
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
        return false;
    fwrite(content.data(), 1, content.size(), f);
    fclose(f);
    return true;
}

static std::string readFile(const std::string &path) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return {};
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        s.append(buf, n);
    fclose(f);
    return s;
}

static bool runOshader(const char *oshaderBin, const std::string &srcPath, const std::string &outPath, bool jit) {
    std::string cmd = std::string("\"") + oshaderBin + "\" " + (jit ? "--jit " : "") +
                      "-o \"" + outPath + "\" \"" + srcPath + "\" >/dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

// Renders ribPath on one thread and returns the shell's status for orender
// (128 + signal for a crash). orender exits non-zero on the unrelated "Failed
// to find shader defaultsurface" path, so only 129..254 means a crash; the
// log is what the checks read.
static int runOrender(const char *orenderBin, const char *displaysDir, const std::string &shadersDir, const std::string &ribPath, const std::string &logPath) {
    std::string cmd = std::string("SHADERS=\"") + shadersDir + "\" ORENDERHOME=\"" + shadersDir +
                      "\" DISPLAYS=\"" + displaysDir + "\" \"" + orenderBin + "\" -t:1 \"" + ribPath +
                      "\" > \"" + logPath + "\" 2>&1; echo \"rc=$?\" >> \"" + logPath + "\"";
    int status = system(cmd.c_str());
    (void)status; // the shell status is read from the log

    const std::string log = readFile(logPath);
    size_t p = log.rfind("rc=");
    return p == std::string::npos ? -1 : atoi(log.c_str() + p + 3);
}

static int countOccurrences(const std::string &haystack, const std::string &needle) {
    int count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

// Lines of `text` that start with `prefix`.
static std::string linesWith(const std::string &text, const std::string &prefix) {
    std::string out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos)
            nl = text.size();
        if (text.compare(pos, prefix.size(), prefix) == 0)
            out.append(text, pos, nl - pos + 1);
        pos = nl + 1;
    }
    return out;
}

static void test_format(const char *oshaderBin, const char *orenderBin, const char *displaysDir, const std::string &format) {
    printf("GitHub #191: unassigned locals under %s\n", format.c_str());

    char tmplBuf[] = "/tmp/shading_parity_uninit_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return;
    const std::string dir = tmpDir;

    // init = 1 dirties the slots, init = 0 reads them unassigned.
    EXPECT_TRUE(writeFile(dir + "/uninit191.sl",
                          "surface uninit191(float init = 0)\n"
                          "{\n"
                          "    string s;\n"
                          "    float f;\n"
                          "    float e = 0;\n"
                          "    if (init > 0.5) { s = \"poison\"; f = 12345; }\n"
                          "    if (s == \"poison\") e = 1;\n"
                          "    printf(\"LEAK init=%f f=%f s=[%s] e=%f\\n\", init, f, s, e);\n"
                          "    color c = trace(P, vector(-1,0,0), \"label\", s);\n"
                          "    Ci = c; Oi = Os;\n"
                          "}\n"));
    EXPECT_TRUE(writeFile(dir + "/label191.sl",
                          "surface label191()\n"
                          "{\n"
                          "    printf(\"LAB=[%s]\\n\", raylabel());\n"
                          "    Ci = color(0,1,0); Oi = Os;\n"
                          "}\n"));
    for (const char *name : {"uninit191", "label191"}) {
        EXPECT_TRUE(runOshader(oshaderBin, dir + "/" + name + ".sl", dir + "/" + name + (format == "slo" ? ".slo" : ".rslo"), format == "slo"));
    }

    // The label191 sphere is the target of the traced rays; the two uninit191
    // spheres are shaded in RIB order.
    const std::string rib = dir + "/scene.rib";
    EXPECT_TRUE(writeFile(rib,
                          "Format 16 16 1\n"
                          "PixelSamples 1 1\n"
                          "Projection \"perspective\" \"fov\" 30\n"
                          "Display \"test.tif\" \"file\" \"rgba\"\n"
                          "Hider \"raytrace\"\n"
                          "WorldBegin\n"
                          "  Attribute \"shade\" \"shaderformat\" [\"" +
                              format + "\"]\n"
                                       "  Attribute \"visibility\" \"int specular\" [1]\n"
                                       "  Attribute \"visibility\" \"int transmission\" [1]\n"
                                       "  Translate 0 0 5\n"
                                       "  Surface \"label191\"\n"
                                       "  AttributeBegin\n    Translate -3 0 0\n    Sphere 2 -2 2 360\n  AttributeEnd\n"
                                       "  Surface \"uninit191\" \"float init\" [1]\n"
                                       "  AttributeBegin\n    Translate 0.8 0.5 0\n    Sphere 0.4 -0.4 0.4 360\n  AttributeEnd\n"
                                       "  Surface \"uninit191\" \"float init\" [0]\n"
                                       "  AttributeBegin\n    Translate 0.8 -0.5 0\n    Sphere 0.4 -0.4 0.4 360\n  AttributeEnd\n"
                                       "WorldEnd\n"));

    const std::string log = dir + "/out.log";
    const int rc = runOrender(orenderBin, displaysDir, dir, rib, log);
    const std::string text = readFile(log);
    printf("  orender status=%d\n", rc);

    // `==`, printf("%s") and trace()'s label read the slot: none may crash.
    EXPECT_TRUE(rc >= 0 && !(rc > 128 && rc < 255));

    const std::string fresh = linesWith(text, "LEAK init=0.000000");
    const std::string dirty = linesWith(text, "LEAK init=1.000000");
    EXPECT_TRUE(!dirty.empty());
    EXPECT_TRUE(!fresh.empty());
    const int nFresh = countOccurrences(fresh, "LEAK");
    // Unassigned float is 0, unassigned string is "" and compares unequal.
    EXPECT_TRUE(countOccurrences(fresh, "f=0.000000 s=[] e=0.000000\n") == nFresh);
    // The assigned instance is unaffected.
    EXPECT_TRUE(countOccurrences(dirty, "f=12345.000000 s=[poison] e=1.000000\n") == countOccurrences(dirty, "LEAK"));
    // A label that came from an unassigned string is "" (the default), never the other instance's.
    EXPECT_TRUE(text.find("LAB=[]") != std::string::npos);
    EXPECT_TRUE(countOccurrences(text, "LAB=[poison]") > 0);
    EXPECT_TRUE(countOccurrences(text, "LAB=[") ==
                countOccurrences(text, "LAB=[camera]") + countOccurrences(text, "LAB=[poison]") + countOccurrences(text, "LAB=[]"));
}

int main() {
    const char *oshaderBin = getenv("OSHADER_BIN");
    const char *orenderBin = getenv("ORENDER_BIN");
    const char *displaysDir = getenv("TEST_DISPLAYS_DIR");
    EXPECT_TRUE(oshaderBin != nullptr);
    EXPECT_TRUE(orenderBin != nullptr);
    EXPECT_TRUE(displaysDir != nullptr);
    if (!oshaderBin || !orenderBin || !displaysDir)
        return 1;

    test_format(oshaderBin, orenderBin, displaysDir, "rslo");
    test_format(oshaderBin, orenderBin, displaysDir, "slo");

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}

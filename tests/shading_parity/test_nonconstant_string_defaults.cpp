/**
 * Project: openRender
 *
 * File: test_nonconstant_string_defaults.cpp
 *
 * Description:
 *   GitHub #192: a shader parameter whose default is not a constant takes its value from the shader's init
 *   code, which runs once at bind time with no shading context. The interpreter runs it; the JIT's init
 *   function ran too, but the context-bound ops it calls (op_concat, op_format, op_shadername) did nothing
 *   without an active context, so a string default such as concat("a", "b") stayed NULL under .slo.
 *
 *   Renders one shader through the orender CLI under .rslo and .slo and checks what it prints. Constant-default
 *   neighbours (before, after) and a numeric non-constant default (k) are canaries: they must be intact.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
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
    printf("GitHub #192: non-constant string defaults under %s\n", format.c_str());

    char tmplBuf[] = "/tmp/shading_parity_strdef_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return;
    const std::string dir = tmpDir;

    EXPECT_TRUE(writeFile(dir + "/dflt192.sl",
                          "surface dflt192(string before = \"B\";\n"
                          "                string s = concat(\"a\", \"b\");\n"
                          "                string t = concat(s, \"c\");\n"
                          "                string f = format(\"%f-%s\", sqrt(4), \"z\");\n"
                          "                string sn = shadername();\n"
                          "                string st = shadername(\"surface\");\n"
                          "                string rl = raylabel();\n"
                          "                string after = \"A\";\n"
                          "                float k = sqrt(9))\n"
                          "{\n"
                          "    printf(\"DEF s=[%s] t=[%s] f=[%s] sn=[%s] st=[%s] rl=[%s] before=[%s] after=[%s] k=%f\\n\", s, t, f, sn, st, rl, before, after, k);\n"
                          "    Ci = 1; Oi = 1;\n"
                          "}\n"));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/dflt192.sl", dir + "/dflt192" + (format == "slo" ? ".slo" : ".rslo"), format == "slo"));

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
                                       "  Translate 0 0 5\n"
                                       "  Surface \"dflt192\"\n"
                                       "  Sphere 1 -1 1 360\n"
                                       "WorldEnd\n"));

    const std::string log = dir + "/out.log";
    const int rc = runOrender(orenderBin, displaysDir, dir, rib, log);
    const std::string text = readFile(log);
    printf("  orender status=%d\n", rc);
    EXPECT_TRUE(rc >= 0 && !(rc > 128 && rc < 255));

    const std::string lines = linesWith(text, "DEF ");
    const int n = countOccurrences(lines, "DEF ");
    printf("  %d shaded points\n", n);
    EXPECT_TRUE(n > 0);
    // Every point sees the computed defaults and the intact neighbours.
    EXPECT_TRUE(countOccurrences(lines, "DEF s=[ab] t=[abc] f=[2.000000-z] sn=[dflt192] st=[] rl=[] before=[B] after=[A] k=3.000000\n") == n);
    EXPECT_TRUE(text.find("(null)") == std::string::npos);
    if (countOccurrences(lines, "DEF s=[ab] t=[abc] f=[2.000000-z] sn=[dflt192] st=[] rl=[] before=[B] after=[A] k=3.000000\n") != n)
        fprintf(stderr, "  first line seen: %s", lines.substr(0, lines.find('\n') + 1).c_str());
}

// The defaults a bind-time init computes must outlive the arena region bind() runs the init in: later RIB statements
// carrying long strings reuse that memory, and a default that pointed into it read their bytes (the interpreter did).
static void test_lifetime(const char *oshaderBin, const char *orenderBin, const char *displaysDir, const std::string &format) {
    printf("GitHub #192: computed defaults survive later statements under %s\n", format.c_str());

    char tmplBuf[] = "/tmp/shading_parity_strlife_XXXXXX";
    char *tmpDir = mkdtemp(tmplBuf);
    EXPECT_TRUE(tmpDir != nullptr);
    if (!tmpDir)
        return;
    const std::string dir = tmpDir;

    EXPECT_TRUE(writeFile(dir + "/life192.sl",
                          "surface life192(string before = \"B\"; string s = concat(\"a\", \"b\"); string t = concat(s, \"c\");\n"
                          "                string f = format(\"%f-%s\", sqrt(4), \"z\"))\n"
                          "{\n"
                          "    printf(\"LIFE s=[%s] t=[%s] f=[%s] before=[%s]\\n\", s, t, f, before);\n"
                          "    Ci = 1; Oi = 1;\n"
                          "}\n"));
    EXPECT_TRUE(runOshader(oshaderBin, dir + "/life192.sl", dir + "/life192" + (format == "slo" ? ".slo" : ".rslo"), format == "slo"));

    const std::string longA(300, 'X');
    const std::string longB(900, 'Y');
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
                                       "  Translate 0 0 5\n"
                                       "  Surface \"life192\"\n"
                                       "  Attribute \"identifier\" \"string name\" [\"" +
                              longA + "\"]\n"
                                      "  AttributeBegin\n    Translate -1.5 0 0\n    Sphere 1 -1 1 360\n  AttributeEnd\n"
                                      "  Surface \"life192\" \"string before\" [\"" +
                              std::string(90, 'Q') + "\"]\n"
                                                     "  Attribute \"identifier\" \"string name\" [\"" +
                              longB + "\"]\n"
                                      "  AttributeBegin\n    Translate 1.5 0 0\n    Sphere 1 -1 1 360\n  AttributeEnd\n"
                                      "WorldEnd\n"));

    const std::string log = dir + "/out.log";
    const int rc = runOrender(orenderBin, displaysDir, dir, rib, log);
    const std::string text = readFile(log);
    printf("  orender status=%d\n", rc);
    EXPECT_TRUE(rc >= 0 && !(rc > 128 && rc < 255));

    const std::string lines = linesWith(text, "LIFE ");
    const int n = countOccurrences(lines, "LIFE ");
    const std::string tail = " s=[ab] t=[abc] f=[2.000000-z] before=[";
    const int first = countOccurrences(lines, "LIFE" + tail + "B]\n");
    const int second = countOccurrences(lines, "LIFE" + tail + std::string(90, 'Q') + "]\n");
    printf("  %d shaded points: %d with the first instance, %d with the second\n", n, first, second);
    // Both spheres are hit, and every point of both reads the computed defaults, not the later statements' bytes
    EXPECT_TRUE(first > 0);
    EXPECT_TRUE(second > 0);
    EXPECT_TRUE(first + second == n);
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
    test_lifetime(oshaderBin, orenderBin, displaysDir, "rslo");
    test_lifetime(oshaderBin, orenderBin, displaysDir, "slo");

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}

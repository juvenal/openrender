// Spec 023 (FR-005): golden table for variable declarations.
//
// Runs parseVariable() over a matrix of declarations (every storage class,
// every type keyword, each array form, with and without a name, plus invalid
// forms) and prints the result and every CVariable field the parser can
// touch. The table captured on the pre-feature code is variable_golden.txt;
// the moved grammar (common/variable.{y,l}) must reproduce it exactly.
//
// Usage: test_common_variables --dump             print the table
//        test_common_variables <golden-file>      compare against the table

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "variable.h"

namespace {

// Field values the parser never writes, so an untouched field shows up as
// exactly this sentinel in the table.
constexpr int kSentinel = -7;

std::string describe(const char *decl, const char *name) {
    CVariable var;
    memset(&var, 0, sizeof(var));
    strcpy(var.name, "<unset>");
    var.numItems = kSentinel;
    var.numFloats = kSentinel;
    var.entry = kSentinel;
    var.usageMarker = kSentinel;
    var.accessor = kSentinel;
    var.type = (EVariableType)kSentinel;
    var.container = (EVariableClass)kSentinel;
    var.storage = (EVariableStorage)kSentinel;

    const int ok = parseVariable(&var, name, decl);

    char line[512];
    snprintf(line, sizeof(line),
             "decl=\"%s\" name=%s%s%s -> ok=%d name=\"%s\" numItems=%d numFloats=%d type=%d "
             "container=%d storage=%d entry=%d usageMarker=%d accessor=%d defaultValue=%s next=%s",
             decl, name ? "\"" : "", name ? name : "NULL", name ? "\"" : "", ok, var.name,
             var.numItems, var.numFloats, (int)var.type, (int)var.container, (int)var.storage,
             var.entry, var.usageMarker, var.accessor, var.defaultValue ? "set" : "null",
             var.next ? "set" : "null");
    return line;
}

std::vector<std::string> table() {
    static const char *classes[] = {"", "global ", "constant ", "uniform ", "varying ", "facevarying ",
                                    "vertex ", "global uniform ", "global varying "};
    static const char *types[] = {"float", "integer", "int", "vector", "color", "normal", "point",
                                  "matrix", "mpoint", "htpoint", "hpoint", "double", "string"};
    static const char *arrays[] = {"", "[1]", "[4]", " [ 3 ] "};
    static const char *names[] = {"", " foo"};

    std::vector<std::string> rows;
    for (const char *c : classes)
        for (const char *t : types)
            for (const char *a : arrays)
                for (const char *n : names) {
                    const std::string decl = std::string(c) + t + a + n;
                    rows.push_back(describe(decl.c_str(), nullptr));
                }

    // The separate name argument overrides (or supplies) the declared name.
    rows.push_back(describe("uniform color", "Cs_given"));
    rows.push_back(describe("varying float[2] inline", "given"));
    rows.push_back(describe("bogus float", "given"));

    // A 62-character name: the longest the lexer accepts (it asserts
    // strlen < VARIABLE_NAME_LENGTH - 1).
    const std::string longName(62, 'n');
    rows.push_back(describe(("uniform float " + longName).c_str(), nullptr));

    // Invalid or unusual forms: each must keep its exact outcome.
    static const char *odd[] = {
        "",
        "uniform",
        "bogus float",
        "uniform bogus",
        "float [",
        "float [4",
        "float []",
        "float [x]",
        "uniform varying float",
        "float float",
        "float x y",
        "color[2.5]",
        "color[0]",
        "   uniform   float   spaced   ",
        "UNIFORM FLOAT",
        "global",
        "constant string[2] s",
        "float 7",
    };
    for (const char *d : odd)
        rows.push_back(describe(d, nullptr));

    return rows;
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s --dump | <golden-file>\n", argv[0]);
        return 2;
    }

    const std::vector<std::string> rows = table();

    if (strcmp(argv[1], "--dump") == 0) {
        for (const std::string &row : rows)
            printf("%s\n", row.c_str());
        return 0;
    }

    std::ifstream golden(argv[1]);
    if (!golden) {
        fprintf(stderr, "cannot open golden file %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(golden, line);)
        expected.push_back(line);

    int mismatches = 0;
    const size_t n = rows.size() > expected.size() ? rows.size() : expected.size();
    for (size_t i = 0; i < n; i++) {
        const std::string got = i < rows.size() ? rows[i] : "<missing>";
        const std::string want = i < expected.size() ? expected[i] : "<missing>";
        if (got != want) {
            if (mismatches < 20)
                fprintf(stderr, "row %zu:\n  expected: %s\n  actual:   %s\n", i + 1, want.c_str(), got.c_str());
            mismatches++;
        }
    }
    if (mismatches) {
        fprintf(stderr, "%d of %zu rows differ from %s\n", mismatches, n, argv[1]);
        return 1;
    }
    printf("all %zu declaration rows match %s\n", rows.size(), argv[1]);
    return 0;
}

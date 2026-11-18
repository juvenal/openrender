/**
 * tests/shading_system/test_metadata_shipped.cpp
 *
 * ShaderMetadata_Shipped (spec 033, US6, FR-016, FR-017, SC-009). describeShaderFile() answers for all 137 shipped shaders in
 * both formats: the type, the name and the parameters (names, storage, types, arrays, outputs) agree with what sloinfo printed
 * on the starting tree (tests/shaders/references/sloinfo137/{rslo,slo}.txt, recorded in Phase 1). The API reads the files and
 * loads nothing: this program links libRSLInfo only, never libshader_shading or libRSLRuntime (a CShader cannot be created
 * and no JIT module compiled from it), and needs no renderer.
 *
 *   argv[1]  the build tree's shaders directory (137 .rslo and, with the JIT, 137 .slo)
 *   argv[2]  tests/shaders/references/sloinfo137
 *   argv[3]  the formats to check: "rslo" or "rslo,slo"
 *
 * Red on the starting tree: libshader/include/openrender/ShaderMetadata.h does not exist (a build failure).
 */
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "libshader/include/openrender/ShaderMetadata.h"

using namespace openrender::shading;

namespace {

    int g_failed = 0, g_passed = 0;
#define EXPECT(cond)                                                      \
    do {                                                                  \
        if (cond)                                                         \
            ++g_passed;                                                   \
        else {                                                            \
            ++g_failed;                                                   \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        }                                                                 \
    } while (0)

    struct SListed {
            std::string type, name;
            std::vector<std::pair<std::string, std::string>> parameters; // name, "[output ]storage type[N]"
    };

    // The recorded listing of one format: "### <format> <name>", "exit N", "stdout:", the listing, "stderr:"
    std::map<std::string, SListed> readListing(const std::string &path) {
        std::map<std::string, SListed> out;
        std::ifstream f(path);
        std::string line, current;
        bool inStdout = false;
        while (std::getline(f, line)) {
            if (line.rfind("### ", 0) == 0) {
                std::istringstream is(line.substr(4));
                std::string format;
                is >> format >> current;
                inStdout = false;
            }
            else if (line == "stdout:")
                inStdout = true;
            else if (line == "stderr:")
                inStdout = false;
            else if (inStdout && !line.empty() && line[0] != '\t') {
                // surface "plastic" (JIT version)
                SListed &s = out[current];
                s.type = line.substr(0, line.find(' '));
                const size_t a = line.find('"'), b = line.find('"', a + 1);
                s.name = line.substr(a + 1, b - a - 1);
            }
            else if (inStdout && line.size() > 2 && line[0] == '\t' && line[1] == '"') {
                // \t"Ka" "uniform float"
                const size_t a = 2, b = line.find('"', a);
                const size_t c = line.find('"', b + 1), d = line.find('"', c + 1);
                out[current].parameters.push_back({line.substr(a, b - a), line.substr(c + 1, d - c - 1)});
            }
        }
        return out;
    }

    std::string describe(const ShaderParameterMetadata &p) {
        std::string s = p.output ? "output " : "";
        s += p.storage + " " + p.typeName;
        if (p.arraySize > 1)
            s += "[" + std::to_string(p.arraySize) + "]";
        return s;
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <shaders dir> <sloinfo137 dir> <rslo|rslo,slo>\n", argv[0]);
        return 2;
    }
    const std::string shaders = argv[1], references = argv[2], formats = argv[3];
    for (const char *format : {"rslo", "slo"}) {
        if (formats.find(format) == std::string::npos)
            continue;
        const std::map<std::string, SListed> listed = readListing(references + "/" + format + ".txt");
        EXPECT(listed.size() == 137);
        for (const auto &entry : listed) {
            const std::string path = shaders + "/" + entry.first + "." + format;
            const ShaderMetadata m = describeShaderFile(path.c_str(), format, nullptr);
            EXPECT(m.status == EMetadataStatus::Found);
            if (m.status != EMetadataStatus::Found) {
                fprintf(stderr, "  %s\n", path.c_str());
                continue;
            }
            EXPECT(m.format == format);
            EXPECT(m.name == entry.second.name);
            EXPECT(m.typeName == entry.second.type);
            EXPECT(m.parameters.size() == entry.second.parameters.size());
            for (size_t i = 0; i < m.parameters.size() && i < entry.second.parameters.size(); i++) {
                EXPECT(m.parameters[i].name == entry.second.parameters[i].first);
                EXPECT(describe(m.parameters[i]) == entry.second.parameters[i].second);
            }
            EXPECT(m.locals.empty()); // none in this spec (sloinfo prints none)
        }
    }
    // a file that is not there is NotFound, and nothing was loaded for it either
    EXPECT(describeShaderFile((shaders + "/no_such_shader_033").c_str(), "rslo", nullptr).status == EMetadataStatus::NotFound);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}

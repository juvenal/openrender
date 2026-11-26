/*
 * Spec 035, Phase 4 (T081), Bxdf_Registry (labels closures, bxdf): the Bxdf registry of contracts/bxdf.md.
 *
 *   - the four built-ins are found on first use through the call graph (no registration call, no static registrar): their names, the table
 *     header (structSize, abiVersion == OR_BXDF_ABI_VERSION) and the number of parameters;
 *   - findBxdf answers {table, reason}: "sl:" and "osl:" prefixes are PrefixReserved, an unknown name Unknown;
 *   - registerBxdf: a valid table registers and is found; a duplicate name refuses the whole table (the first stays); a table of an unsupported
 *     MAJOR is refused AND recorded, so a later lookup of its name answers VersionRefused carrying that MAJOR; a table whose structSize is too
 *     small is refused (the table-size rule);
 *   - the header carries the version history table (1.0.0, Spec 035).
 *
 * Red on the starting tree: does not compile (bxdfAbi.h and bxdfRegistry.h do not exist).
 */
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "bxdfAbi.h"
#include "bxdfRegistry.h"
#include "closures/test_support.h"

using namespace openrender::shading;

namespace {

    OrBxdfData *createNone(const void *const *, const OrBxdfDiagnostics *) { return nullptr; }
    void destroyNone(OrBxdfData *) {}
    const OrClosureNode *buildNone(const OrBxdfData *, const OrBxdfPoint *, const OrClosureAllocator *) { return nullptr; }

    OrBxdfTable table(const char *name, uint32_t version = OR_BXDF_ABI_VERSION, uint32_t size = sizeof(OrBxdfTable)) {
        OrBxdfTable t;
        std::memset(&t, 0, sizeof(t));
        t.header.structSize = size;
        t.header.abiVersion = version;
        t.name = name;
        t.params = nullptr;
        t.numParams = 0;
        t.create = createNone;
        t.destroy = destroyNone;
        t.build = buildNone;
        return t;
    }

    void builtins() {
        const struct {
                const char *name;
                int params;
        } expected[] = {{"diffuse", 2}, {"plastic", 5}, {"conductor", 2}, {"dielectric", 4}};
        for (const auto &e : expected) {
            const SBxdfLookup l = findBxdf(e.name);
            CL_CHECK(l.table != nullptr && l.reason == BxdfLookupReason::Found);
            if (l.table == nullptr)
                continue;
            CL_CHECK(std::strcmp(l.table->name, e.name) == 0);
            CL_CHECK(l.table->header.structSize == sizeof(OrBxdfTable));
            CL_CHECK(l.table->header.abiVersion == OR_BXDF_ABI_VERSION);
            CL_CHECK_INT(l.table->numParams, e.params);
            CL_CHECK(l.table->create != nullptr && l.table->destroy != nullptr && l.table->build != nullptr);
        }
    }

    void lookups() {
        for (const char *name : {"sl:myshader", "osl:myshader", "sl:", "osl:"}) {
            const SBxdfLookup l = findBxdf(name);
            CL_CHECK(l.table == nullptr && l.reason == BxdfLookupReason::PrefixReserved);
        }
        const SBxdfLookup unknown = findBxdf("p4_no_such_bxdf");
        CL_CHECK(unknown.table == nullptr && unknown.reason == BxdfLookupReason::Unknown);
        // case matters and a prefix is only a prefix
        CL_CHECK(findBxdf("Diffuse").reason == BxdfLookupReason::Unknown);
        CL_CHECK(findBxdf("xsl:diffuse").reason == BxdfLookupReason::Unknown);
    }

    void registration() {
        static OrBxdfTable good = table("p4_good");
        CL_CHECK(registerBxdf(&good, "test_bxdf_registry"));
        const SBxdfLookup l = findBxdf("p4_good");
        CL_CHECK(l.table == &good && l.reason == BxdfLookupReason::Found);

        // a duplicate name: the whole second table is refused, the first stays
        static OrBxdfTable again = table("p4_good");
        CL_CHECK(!registerBxdf(&again, "test_bxdf_registry"));
        CL_CHECK(findBxdf("p4_good").table == &good);
        static OrBxdfTable builtinAgain = table("diffuse");
        CL_CHECK(!registerBxdf(&builtinAgain, "test_bxdf_registry"));
        CL_CHECK(findBxdf("diffuse").table != &builtinAgain);

        // an unsupported MAJOR: refused and recorded with its reason
        static OrBxdfTable future = table("p4_future", OR_ABI_VERSION(2, 0, 0));
        CL_CHECK(!registerBxdf(&future, "test_bxdf_registry"));
        const SBxdfLookup refused = findBxdf("p4_future");
        CL_CHECK(refused.table == nullptr && refused.reason == BxdfLookupReason::VersionRefused && refused.major == 2);
        // a newer MINOR of the supported MAJOR loads (with a warning)
        static OrBxdfTable newer = table("p4_newer_minor", OR_ABI_VERSION(1, 1, 0));
        CL_CHECK(registerBxdf(&newer, "test_bxdf_registry"));
        CL_CHECK(findBxdf("p4_newer_minor").table == &newer);

        // the table-size rule: a table smaller than the header it declares is refused
        static OrBxdfTable shortTable = table("p4_short", OR_BXDF_ABI_VERSION, (uint32_t)sizeof(OrTableHeader));
        CL_CHECK(!registerBxdf(&shortTable, "test_bxdf_registry"));
        CL_CHECK(findBxdf("p4_short").table == nullptr);
    }

    void header() {
        std::ifstream f(std::string(CL_SOURCE_DIR) + "/src/libshader/closures/bxdfAbi.h");
        std::stringstream ss;
        ss << f.rdbuf();
        const std::string text = ss.str();
        CL_CHECK(!text.empty());
        CL_CHECK(text.find("Version history") != std::string::npos || text.find("version history") != std::string::npos);
        CL_CHECK(text.find("1.0.0") != std::string::npos && text.find("Spec 035") != std::string::npos);
    }

} // namespace

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "Builtins")
        builtins();
    else if (which == "Lookups")
        lookups();
    else if (which == "Registration")
        registration();
    else if (which == "Header")
        header();
    else {
        std::printf("usage: test_bxdf_registry <Builtins|Lookups|Registration|Header>\n");
        return 2;
    }
    return CL_RESULT(("Bxdf_Registry_" + which).c_str());
}

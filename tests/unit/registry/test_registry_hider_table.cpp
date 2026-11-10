/**
 * Project: openRender
 *
 * File: test_registry_hider_table.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), FR-001, FR-002, FR-004, FR-011, FR-014, FR-015.
 *   contracts/hider-registration.md.
 *
 *     Registry_HiderLookup      names and aliases resolve (exact, case-sensitive); the plain alias carries no
 *                               warning, the deprecated one carries the exact text; enumeration lists the four
 *                               built-ins in the fixed order; the declared capabilities of each built-in; the
 *                               default (unknown-name fallback) is reyes.
 *     Registry_HiderCollisions  on a LOCAL registry (the process one is never touched): a duplicate name, an
 *                               alias equal to a name, an alias equal to an alias and a table without a name are
 *                               refused with one diagnostic each and the first registration stays; a table of
 *                               MAJOR 3 and a table smaller than OrHiderTable are refused (the version check of
 *                               the registry, Q-B) and not registered; a non-NULL pair field of an alias (FR-014)
 *                               warns once at registration, the alias is kept and selects the hider only.
 *
 *   Red by build failure until Phase 6 (no hiderRegistry.h). The case to run is the first argument.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <cstring>
#include <string>

#include "common/diagnostics.h"
#include "hiderAbi.h"
#include "hiderRegistry.h"
#include "registry_testing.h"
#include "transport/capability.h"

using namespace regtest;

namespace {

    const uint32_t kV100 = OR_ABI_VERSION(1, 0, 0);

    // A table that creates nothing: the registry validates and stores, it never calls create here.
    OrHiderInstance *createNothing(const OrHiderCreateInfo *) {
        return nullptr;
    }
    void destroyNothing(OrHiderInstance *) {}

    OrHiderTable makeTable(const char *name, const OrHiderAlias *aliases, int numAliases) {
        OrHiderTable t{};
        t.header.structSize = sizeof(OrHiderTable);
        t.header.abiVersion = OR_HIDER_ABI_VERSION;
        t.name = name;
        t.aliases = aliases;
        t.numAliases = numAliases;
        t.provides = nullptr;
        t.numProvides = 0;
        t.dispatch = OR_HIDER_DISPATCH_BUCKETS;
        t.preDisplaySetup = nullptr;
        t.create = createNothing;
        t.destroy = destroyNothing;
        return t;
    }

    bool provides(const OrHiderTable *t, const char *capability) {
        uint32_t version = 0;
        return hiderProvides(t, capability, &version) && version == kV100;
    }

    // Registry_HiderLookup
    void lookup() {
        CRecordingSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);
        CHiderRegistry &registry = hiderRegistry();
        installDiagnosticSink(previous);
        REG_CHECK_MSG(sink.reports.list.empty(), "registering the built-ins reports nothing; got:\n%s", sink.reports.text().c_str());

        // Enumeration: the four built-ins, in the fixed order (this program links no other hider).
        REG_CHECK_MSG(registry.size() == 4, "size %d", registry.size());
        const char *order[] = {"raytrace", "reyes", "zbuffer", "photon"};
        for (int i = 0; i < 4 && i < registry.size(); ++i)
            REG_CHECK_MSG(registry.at(i) != nullptr && std::strcmp(registry.at(i)->name, order[i]) == 0, "entry %d is %s", i,
                          registry.at(i) != nullptr ? registry.at(i)->name : "(null)");

        // Names resolve to their own table, with no alias.
        for (const char *name : order) {
            const CHiderRegistry::SLookup l = registry.find(name);
            REG_CHECK_MSG(l.table != nullptr && l.alias == nullptr, "%s resolves as a name", name);
            REG_CHECK(l.table != nullptr && std::strcmp(l.table->name, name) == 0);
        }

        // Aliases: "hidden" is plain, "stochastic" is deprecated with the exact text; both select reyes.
        const CHiderRegistry::SLookup hidden = registry.find("hidden");
        REG_CHECK(hidden.table != nullptr && hidden.alias != nullptr);
        REG_CHECK(hidden.table != nullptr && std::strcmp(hidden.table->name, "reyes") == 0);
        REG_CHECK(hidden.alias != nullptr && hidden.alias->deprecationWarning == nullptr);
        REG_CHECK(hidden.alias != nullptr && hidden.alias->integrator == nullptr);
        const CHiderRegistry::SLookup stochastic = registry.find("stochastic");
        REG_CHECK(stochastic.table != nullptr && stochastic.alias != nullptr);
        REG_CHECK(stochastic.table != nullptr && std::strcmp(stochastic.table->name, "reyes") == 0);
        REG_CHECK(stochastic.alias != nullptr && stochastic.alias->deprecationWarning != nullptr);
        REG_CHECK(stochastic.alias != nullptr && stochastic.alias->deprecationWarning != nullptr &&
                  std::strstr(stochastic.alias->deprecationWarning, "Deprecated Hider \"stochastic\"; use \"reyes\" instead") != nullptr);

        // Exact and case-sensitive: nothing else resolves.
        REG_CHECK(registry.find("Reyes").table == nullptr);
        REG_CHECK(registry.find("REYES").table == nullptr);
        REG_CHECK(registry.find("bogus").table == nullptr);
        REG_CHECK(registry.find("").table == nullptr);
        REG_CHECK(registry.find(nullptr).table == nullptr);

        // The unknown-name fallback is the reyes entry.
        REG_CHECK(defaultHider() != nullptr && std::strcmp(defaultHider()->name, "reyes") == 0);
        REG_CHECK(defaultHider() == registry.find("reyes").table);

        // Declared capabilities (all 1.0.0), contracts/hider-registration.md "Built-in registrations".
        const OrHiderTable *raytrace = registry.find("raytrace").table;
        const OrHiderTable *reyes = registry.find("reyes").table;
        const OrHiderTable *zbuffer = registry.find("zbuffer").table;
        const OrHiderTable *photon = registry.find("photon").table;
        REG_CHECK(raytrace && provides(raytrace, OR_CAP_DISPLAY_OUTPUT) && provides(raytrace, OR_CAP_DISPLAY_CHANNELS));
        REG_CHECK(raytrace && !provides(raytrace, OR_CAP_RASTERIZATION) && !provides(raytrace, OR_CAP_DEEP_SHADOW_OUTPUT));
        REG_CHECK(reyes && provides(reyes, OR_CAP_RASTERIZATION) && provides(reyes, OR_CAP_DISPLAY_OUTPUT) &&
                  provides(reyes, OR_CAP_DISPLAY_CHANNELS) && provides(reyes, OR_CAP_DEEP_SHADOW_OUTPUT));
        REG_CHECK(zbuffer && provides(zbuffer, OR_CAP_RASTERIZATION) && provides(zbuffer, OR_CAP_DISPLAY_OUTPUT));
        REG_CHECK(zbuffer && !provides(zbuffer, OR_CAP_DISPLAY_CHANNELS) && !provides(zbuffer, OR_CAP_DEEP_SHADOW_OUTPUT));
        REG_CHECK(photon && provides(photon, OR_CAP_ILLUMINATION_HOOK));
        REG_CHECK(photon && !provides(photon, OR_CAP_DISPLAY_OUTPUT) && !provides(photon, OR_CAP_RASTERIZATION));
        REG_CHECK(!hiderProvides(nullptr, OR_CAP_RASTERIZATION, nullptr)); // nullptr-safe
        REG_CHECK(raytrace && reyes && zbuffer && photon);

        // Dispatch kinds.
        REG_CHECK(photon && photon->dispatch == OR_HIDER_DISPATCH_PHOTONS);
        REG_CHECK(reyes && reyes->dispatch == OR_HIDER_DISPATCH_BUCKETS);
        REG_CHECK(raytrace && raytrace->dispatch == OR_HIDER_DISPATCH_BUCKETS);
        REG_CHECK(zbuffer && zbuffer->dispatch == OR_HIDER_DISPATCH_BUCKETS);

        // Every built-in declares the registry's own table version.
        for (int i = 0; i < registry.size(); ++i) {
            REG_CHECK(registry.at(i)->header.abiVersion == OR_HIDER_ABI_VERSION);
            REG_CHECK(registry.at(i)->header.structSize >= sizeof(OrHiderTable));
        }
    }

    // Registry_HiderCollisions
    void collisions() {
        CRecordingSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);

        const OrHiderAlias firstAlias[] = {{"first-alias", nullptr, nullptr}};
        const OrHiderTable first = makeTable("first", firstAlias, 1);
        CHiderRegistry registry; // a local instance: the process registry is not touched
        REG_CHECK(registry.add(&first, "test first"));
        REG_CHECK(registry.size() == 1);
        REG_CHECK_MSG(sink.reports.list.empty(), "a valid table reports nothing; got:\n%s", sink.reports.text().c_str());
        REG_CHECK(registry.find("first").table == &first);
        REG_CHECK(registry.find("first-alias").table == &first && registry.find("first-alias").alias == &firstAlias[0]);

        // 1. A duplicate name: refused, one error naming both entries, the first stays.
        const OrHiderTable duplicate = makeTable("first", nullptr, 0);
        REG_CHECK(!registry.add(&duplicate, "test duplicate"));
        REG_CHECK_MSG(sink.reports.count("Hider \"first\" is already registered (by \"first\"); the second registration is ignored") == 1,
                      "got:\n%s", sink.reports.text().c_str());
        REG_CHECK(sink.reports.errors() == 1);
        REG_CHECK(registry.size() == 1);
        REG_CHECK(registry.find("first").table == &first);

        // 2. An alias equal to a registered name: the table is refused, nothing of it is registered.
        sink.reports.clear();
        const OrHiderAlias namesFirst[] = {{"first", nullptr, nullptr}};
        const OrHiderTable aliasIsName = makeTable("second", namesFirst, 1);
        REG_CHECK(!registry.add(&aliasIsName, "test alias = name"));
        REG_CHECK_MSG(sink.reports.count("\"first\" is already registered") == 1, "got:\n%s", sink.reports.text().c_str());
        REG_CHECK(sink.reports.errors() == 1);
        REG_CHECK(registry.find("second").table == nullptr);
        REG_CHECK(registry.find("first").table == &first);
        REG_CHECK(registry.size() == 1);

        // 3. An alias equal to another table's alias.
        sink.reports.clear();
        const OrHiderAlias sameAlias[] = {{"first-alias", nullptr, nullptr}};
        const OrHiderTable aliasIsAlias = makeTable("third", sameAlias, 1);
        REG_CHECK(!registry.add(&aliasIsAlias, "test alias = alias"));
        REG_CHECK_MSG(sink.reports.count("\"first-alias\" is already registered") == 1, "got:\n%s", sink.reports.text().c_str());
        REG_CHECK(sink.reports.errors() == 1);
        REG_CHECK(registry.find("third").table == nullptr);
        REG_CHECK(registry.find("first-alias").table == &first);

        // 4. A name equal to a registered alias.
        sink.reports.clear();
        const OrHiderTable nameIsAlias = makeTable("first-alias", nullptr, 0);
        REG_CHECK(!registry.add(&nameIsAlias, "test name = alias"));
        REG_CHECK(sink.reports.errors() == 1);
        REG_CHECK(registry.size() == 1);

        // 5. A table without a name (NULL, then empty): the only way to have an alias without a target.
        sink.reports.clear();
        const OrHiderAlias orphan[] = {{"orphan", nullptr, nullptr}};
        const OrHiderTable noName = makeTable(nullptr, orphan, 1);
        REG_CHECK(!registry.add(&noName, "test unnamed"));
        REG_CHECK_MSG(sink.reports.count("Hider registration without a name ignored (test unnamed)") == 1, "got:\n%s", sink.reports.text().c_str());
        const OrHiderTable emptyName = makeTable("", orphan, 1);
        REG_CHECK(!registry.add(&emptyName, "test empty name"));
        REG_CHECK(sink.reports.count("Hider registration without a name ignored") == 2);
        REG_CHECK(registry.find("orphan").table == nullptr);
        REG_CHECK(registry.size() == 1);

        // 6. The version check (Q-B): a table of MAJOR 3 is refused, with a CODE_VERSION report, and not registered.
        sink.reports.clear();
        OrHiderTable major3 = makeTable("major3", nullptr, 0);
        major3.header.abiVersion = OR_ABI_VERSION(3, 0, 0);
        REG_CHECK(!registry.add(&major3, "test major 3"));
        REG_CHECK_MSG(sink.reports.errors() >= 1, "a rejected version is reported; got:\n%s", sink.reports.text().c_str());
        bool versionCode = false;
        for (const SReport &r : sink.reports.list)
            if (r.code == CODE_VERSION)
                versionCode = true;
        REG_CHECK_MSG(versionCode, "the report carries CODE_VERSION; got:\n%s", sink.reports.text().c_str());
        REG_CHECK(registry.find("major3").table == nullptr);

        // 7. The table size rule: a table smaller than OrHiderTable is refused and not registered.
        sink.reports.clear();
        OrHiderTable small = makeTable("small", nullptr, 0);
        small.header.structSize = sizeof(OrTableHeader);
        REG_CHECK(!registry.add(&small, "test too small"));
        REG_CHECK_MSG(sink.reports.errors() >= 1, "a too-small table is reported; got:\n%s", sink.reports.text().c_str());
        versionCode = false;
        for (const SReport &r : sink.reports.list)
            if (r.code == CODE_VERSION)
                versionCode = true;
        REG_CHECK_MSG(versionCode, "the report carries CODE_VERSION; got:\n%s", sink.reports.text().c_str());
        REG_CHECK(registry.find("small").table == nullptr);

        // 8. A newer MINOR of the same MAJOR loads (one warning at most): it is registered.
        sink.reports.clear();
        OrHiderTable newerMinor = makeTable("newer-minor", nullptr, 0);
        newerMinor.header.abiVersion = OR_ABI_VERSION(OR_ABI_MAJOR(OR_HIDER_ABI_VERSION), OR_ABI_MINOR(OR_HIDER_ABI_VERSION) + 1, 0);
        REG_CHECK(registry.add(&newerMinor, "test newer minor"));
        REG_CHECK(registry.find("newer-minor").table == &newerMinor);

        // 9. The reserved pair field (FR-014): a non-NULL `integrator` warns once at registration, the alias is
        //    kept and selects the hider only.
        sink.reports.clear();
        const OrHiderAlias pair[] = {{"pair", nullptr, "classic"}};
        const OrHiderTable pairTable = makeTable("pairs", pair, 1);
        REG_CHECK(registry.add(&pairTable, "test pair alias"));
        REG_CHECK_MSG(sink.reports.count("Hider alias \"pair\" names integrator \"classic\"; pair aliases are not supported yet, the alias selects the "
                                         "hider only") == 1,
                      "got:\n%s", sink.reports.text().c_str());
        REG_CHECK(sink.reports.warnings() == 1 && sink.reports.errors() == 0);
        bool badToken = false;
        for (const SReport &r : sink.reports.list)
            if (r.code == CODE_BADTOKEN)
                badToken = true;
        REG_CHECK(badToken);
        REG_CHECK(registry.find("pair").table == &pairTable && registry.find("pair").alias == &pair[0]);
        sink.reports.clear();
        (void)registry.find("pair"); // a lookup never warns again
        REG_CHECK(sink.reports.list.empty());

        // 10. Enumeration order is registration order.
        REG_CHECK(registry.size() == 3);
        REG_CHECK(registry.at(0) == &first && registry.at(1) == &newerMinor && registry.at(2) == &pairTable);

        installDiagnosticSink(previous);
    }

} // namespace

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "lookup")
        lookup();
    else if (which == "collisions")
        collisions();
    else {
        std::fprintf(stderr, "usage: %s lookup|collisions\n", argv[0]);
        return 2;
    }
    return finish(which.c_str());
}

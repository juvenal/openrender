/**
 * Project: openRender
 *
 * File: test_registry_integrator_table.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), FR-010, FR-012, FR-015, FR-016.
 *   contracts/integrator-interface.md.
 *
 *     Registry_IntegratorLookup  `classic` is registered, resolves by name, is the default of a fresh options
 *                                block and the only entry this program links; enumeration lists it; on a LOCAL
 *                                registry a duplicate name, an alias collision and a table without a name are
 *                                refused with one diagnostic each, the first stays; a table of MAJOR 3 and a table
 *                                smaller than OrIntegratorTable are refused by the version check (Q-B) and not
 *                                registered.
 *     Registry_PairCheck         transportCheckPair(), the pure function: accept; a missing capability; a version
 *                                shortfall (both versions in the verdict); an absent or older optional capability
 *                                is listed and does not refuse; several shortfalls are all listed.
 *
 *   Red by build failure until Phase 5 (no transport/ headers). The case to run is the first argument.
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
#include "options.h"
#include "registry_testing.h"
#include "ri.h"
#include "riInterface.h"
#include "transport/capability.h"
#include "transport/capabilityCheck.h"
#include "transport/classicIntegrator.h"
#include "transport/integratorAbi.h"
#include "transport/integratorRegistry.h"

using namespace regtest;

namespace {

    const uint32_t kV100 = OR_ABI_VERSION(1, 0, 0);
    const uint32_t kV200 = OR_ABI_VERSION(2, 0, 0);

    OrIntegratorInstance *createNothing(void) {
        return nullptr;
    }
    void destroyNothing(OrIntegratorInstance *) {}
    OrStatus configureNothing(OrIntegratorInstance *, const OrIntegratorSettings *, int *) {
        return OR_STATUS_OK;
    }
    OrStatus prepareNothing(OrIntegratorInstance *, const OrTransportHost *, const OrIntegratorFrame *) {
        return OR_STATUS_OK;
    }
    void finishNothing(OrIntegratorInstance *, const OrTransportHost *) {}

    OrIntegratorTable makeTable(const char *name, const OrIntegratorAlias *aliases, int numAliases) {
        OrIntegratorTable t{};
        t.header.structSize = sizeof(OrIntegratorTable);
        t.header.abiVersion = OR_INTEGRATOR_ABI_VERSION;
        t.name = name;
        t.aliases = aliases;
        t.numAliases = numAliases;
        t.required = nullptr;
        t.numRequired = 0;
        t.optional = nullptr;
        t.numOptional = 0;
        t.create = createNothing;
        t.destroy = destroyNothing;
        t.configure = configureNothing;
        t.prepare = prepareNothing;
        t.finish = finishNothing;
        return t;
    }

    bool hasCode(const SReports &r, int code) {
        for (const SReport &x : r.list)
            if (x.code == code)
                return true;
        return false;
    }

    // Registry_IntegratorLookup
    void lookup() {
        CRecordingSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);

        // The process registry: `classic`, registered and found by name.
        CIntegratorRegistry &registry = integratorRegistry();
        REG_CHECK_MSG(sink.reports.list.empty(), "registering classic reports nothing; got:\n%s", sink.reports.text().c_str());
        REG_CHECK_MSG(registry.size() == 1, "size %d", registry.size());
        REG_CHECK(registry.at(0) != nullptr && std::strcmp(registry.at(0)->name, "classic") == 0);
        const CIntegratorRegistry::SLookup classic = registry.find("classic");
        REG_CHECK(classic.table != nullptr && classic.alias == nullptr);
        REG_CHECK(classic.table == orIntegratorTableClassic());
        REG_CHECK(classic.table != nullptr && classic.table->numRequired == 0 && classic.table->numOptional == 0);
        REG_CHECK(classic.table != nullptr && classic.table->header.abiVersion == OR_INTEGRATOR_ABI_VERSION);
        REG_CHECK(registry.find("Classic").table == nullptr); // exact, case-sensitive
        REG_CHECK(registry.find("bogus").table == nullptr);
        REG_CHECK(registry.find("").table == nullptr);
        REG_CHECK(registry.find(nullptr).table == nullptr);

        // `classic` is the default of a fresh options block, and `Integrator "classic"` stores the same name.
        installDiagnosticSink(previous);
        RiLastError = RIE_NOERROR;
        RiBegin(RI_NULL);
        {
            COptions *o = static_cast<CRiInterface *>(const_cast<void *>(RiGetContext()))->getOptions();
            REG_CHECK(o->integrator != nullptr && std::strcmp(o->integrator, "classic") == 0);
            REG_CHECK(o->numIntegratorParameters == 0);
        }
        RiEnd();
        previous = installDiagnosticSink(&sink);

        // A local registry: collisions and rejections (the process registry is not touched).
        const OrIntegratorAlias oldNames[] = {{"first-alias", nullptr}};
        const OrIntegratorTable first = makeTable("first", oldNames, 1);
        CIntegratorRegistry local;
        sink.reports.clear();
        REG_CHECK(local.add(&first, "test first"));
        REG_CHECK(sink.reports.list.empty());
        REG_CHECK(local.find("first-alias").table == &first && local.find("first-alias").alias == &oldNames[0]);

        const OrIntegratorTable duplicate = makeTable("first", nullptr, 0);
        REG_CHECK(!local.add(&duplicate, "test duplicate"));
        REG_CHECK_MSG(
            sink.reports.count("Integrator \"first\" is already registered (by \"first\"); the second registration is ignored") == 1,
            "got:\n%s", sink.reports.text().c_str());
        REG_CHECK(sink.reports.errors() == 1);

        sink.reports.clear();
        const OrIntegratorAlias namesFirst[] = {{"first", nullptr}};
        const OrIntegratorTable aliasIsName = makeTable("second", namesFirst, 1);
        REG_CHECK(!local.add(&aliasIsName, "test alias = name"));
        REG_CHECK(sink.reports.count("\"first\" is already registered") == 1);
        REG_CHECK(local.find("second").table == nullptr);

        sink.reports.clear();
        const OrIntegratorAlias sameAlias[] = {{"first-alias", nullptr}};
        const OrIntegratorTable aliasIsAlias = makeTable("third", sameAlias, 1);
        REG_CHECK(!local.add(&aliasIsAlias, "test alias = alias"));
        REG_CHECK(sink.reports.count("\"first-alias\" is already registered") == 1);
        REG_CHECK(local.find("third").table == nullptr);

        sink.reports.clear();
        const OrIntegratorTable noName = makeTable(nullptr, nullptr, 0);
        REG_CHECK(!local.add(&noName, "test unnamed"));
        REG_CHECK(sink.reports.count("Integrator registration without a name ignored (test unnamed)") == 1);
        REG_CHECK(local.size() == 1);

        // The version check (Q-B): MAJOR 3 and a too-small structSize are refused and not registered.
        sink.reports.clear();
        OrIntegratorTable major3 = makeTable("major3", nullptr, 0);
        major3.header.abiVersion = OR_ABI_VERSION(3, 0, 0);
        REG_CHECK(!local.add(&major3, "test major 3"));
        REG_CHECK_MSG(sink.reports.errors() >= 1 && hasCode(sink.reports, CODE_VERSION), "got:\n%s", sink.reports.text().c_str());
        REG_CHECK(local.find("major3").table == nullptr);

        sink.reports.clear();
        OrIntegratorTable small = makeTable("small", nullptr, 0);
        small.header.structSize = sizeof(OrTableHeader);
        REG_CHECK(!local.add(&small, "test too small"));
        REG_CHECK_MSG(sink.reports.errors() >= 1 && hasCode(sink.reports, CODE_VERSION), "got:\n%s", sink.reports.text().c_str());
        REG_CHECK(local.find("small").table == nullptr);
        REG_CHECK(local.size() == 1);

        installDiagnosticSink(previous);
    }

    // Registry_PairCheck
    void pairCheck() {
        const OrCapability provided[] = {{OR_CAP_RASTERIZATION, kV100}, {OR_CAP_DISPLAY_OUTPUT, kV100}};

        // accept: nothing required, nothing optional
        {
            const SPairVerdict v = transportCheckPair(provided, 2, nullptr, 0, nullptr, 0);
            REG_CHECK(v.accepted && v.shortfalls.empty() && v.absentOptional.empty());
        }
        // accept: a required capability that is provided, at the same version and at a lower required version
        {
            const OrCapability required[] = {{OR_CAP_RASTERIZATION, kV100}};
            const SPairVerdict v = transportCheckPair(provided, 2, required, 1, nullptr, 0);
            REG_CHECK(v.accepted && v.shortfalls.empty());
            const OrCapability newer[] = {{OR_CAP_RASTERIZATION, OR_ABI_VERSION(0, 9, 0)}};
            REG_CHECK(transportCheckPair(provided, 2, newer, 1, nullptr, 0).accepted);
        }
        // refuse: a missing capability, with its name and the version required
        {
            const OrCapability required[] = {{OR_CAP_ILLUMINATION_HOOK, kV100}};
            const SPairVerdict v = transportCheckPair(provided, 2, required, 1, nullptr, 0);
            REG_CHECK(!v.accepted);
            REG_CHECK(v.shortfalls.size() == 1);
            if (v.shortfalls.size() == 1) {
                REG_CHECK(std::strcmp(v.shortfalls[0].name, OR_CAP_ILLUMINATION_HOOK) == 0);
                REG_CHECK(v.shortfalls[0].missing);
                REG_CHECK(v.shortfalls[0].required == kV100);
            }
        }
        // refuse: a version shortfall carries both versions
        {
            const OrCapability required[] = {{OR_CAP_RASTERIZATION, kV200}};
            const SPairVerdict v = transportCheckPair(provided, 2, required, 1, nullptr, 0);
            REG_CHECK(!v.accepted);
            REG_CHECK(v.shortfalls.size() == 1);
            if (v.shortfalls.size() == 1) {
                REG_CHECK(std::strcmp(v.shortfalls[0].name, OR_CAP_RASTERIZATION) == 0);
                REG_CHECK(!v.shortfalls[0].missing);
                REG_CHECK(v.shortfalls[0].required == kV200);
                REG_CHECK(v.shortfalls[0].provided == kV100);
            }
        }
        // refuse: every shortfall is listed, in the order of the requirement
        {
            const OrCapability required[] = {{OR_CAP_RASTERIZATION, kV200}, {OR_CAP_DEEP_SHADOW_OUTPUT, kV100}, {OR_CAP_DISPLAY_OUTPUT, kV100}};
            const SPairVerdict v = transportCheckPair(provided, 2, required, 3, nullptr, 0);
            REG_CHECK(!v.accepted && v.shortfalls.size() == 2);
            if (v.shortfalls.size() == 2) {
                REG_CHECK(std::strcmp(v.shortfalls[0].name, OR_CAP_RASTERIZATION) == 0 && !v.shortfalls[0].missing);
                REG_CHECK(std::strcmp(v.shortfalls[1].name, OR_CAP_DEEP_SHADOW_OUTPUT) == 0 && v.shortfalls[1].missing);
            }
        }
        // accept with optional capabilities absent or older: they are listed and do not refuse (US4 AS4)
        {
            const OrCapability optional[] = {{OR_CAP_ILLUMINATION_HOOK, kV100}, {OR_CAP_DISPLAY_OUTPUT, kV200}, {OR_CAP_RASTERIZATION, kV100}};
            const SPairVerdict v = transportCheckPair(provided, 2, nullptr, 0, optional, 3);
            REG_CHECK(v.accepted && v.shortfalls.empty());
            REG_CHECK(v.absentOptional.size() == 2);
            if (v.absentOptional.size() == 2) {
                REG_CHECK(std::strcmp(v.absentOptional[0], OR_CAP_ILLUMINATION_HOOK) == 0);
                REG_CHECK(std::strcmp(v.absentOptional[1], OR_CAP_DISPLAY_OUTPUT) == 0);
            }
        }
        // a hider that provides nothing
        {
            const OrCapability required[] = {{OR_CAP_RASTERIZATION, kV100}};
            const SPairVerdict v = transportCheckPair(nullptr, 0, required, 1, nullptr, 0);
            REG_CHECK(!v.accepted && v.shortfalls.size() == 1 && v.shortfalls[0].missing);
        }
    }

} // namespace

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "lookup")
        lookup();
    else if (which == "pair_check")
        pairCheck();
    else {
        std::fprintf(stderr, "usage: %s lookup|pair_check\n", argv[0]);
        return 2;
    }
    return finish(which.c_str());
}

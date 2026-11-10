/**
 * Project: openRender
 *
 * File: test_registry_hider_double.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), US2 AS1-AS2, FR-003, SC-002. Registry_TestDoubleHider.
 *
 *   A hider defined and registered (OR_REGISTER_HIDER) in a file of tests/unit/registry, outside the renderer
 *   core and outside src/ri/hiders, is selectable by name and by alias from RIB and renders a frame:
 *     - it is in the registry after the four built-ins, name and aliases resolving to it;
 *     - Hider "testdouble" builds it once per render thread (the thread numbers 0..n-1, the thread count and the
 *       frame's peer group handed to create), runs its pre-display setup once, hands it every bucket job of the
 *       frame (its dispatch kind is BUCKETS) and destroys every instance through the table;
 *     - the plain alias stores the canonical name and is silent; the deprecated alias stores the canonical name
 *       and warns once per statement with the alias's own text;
 *     - no error is reported.
 *   That the core names no hider is Registry_CoreNamesNoHider's job. Red by assertion until Phase 8 (the core
 *   still chooses by name: Hider "testdouble" is unavailable and reyes renders instead).
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

#include "hiderRegistry.h"
#include "hider_double.h"
#include "options.h"
#include "registry_testing.h"
#include "ri.h"
#include "riInterface.h"

using namespace regtest;

namespace {

    COptions *options() {
        return static_cast<CRiInterface *>(const_cast<void *>(RiGetContext()))->getOptions();
    }

    SSession renderWith(const char *hider, SHiderDoubleCounts &counts, const char *tag) {
        counts.reset();
        SFrame f;
        f.threads = 2;
        f.statements = std::string("  Hider \"") + hider + "\"\n";
        f.image = std::string("hider_double_") + tag + ".tif";
        removeFile(f.image);
        return renderRib(frameText(f), std::string("hider_double_") + tag + ".rib");
    }

} // namespace

int main() {
    SHiderDoubleCounts &counts = hiderDoubleCounts();

    // 1. The registry: four built-ins, then the double (registered by its file's static registrar).
    {
        CHiderRegistry &registry = hiderRegistry();
        REG_CHECK_MSG(registry.size() == 6, "size %d (4 built-ins + 2 doubles)", registry.size());
        const char *builtins[] = {"raytrace", "reyes", "zbuffer", "photon"};
        for (int i = 0; i < 4 && i < registry.size(); ++i)
            REG_CHECK(std::strcmp(registry.at(i)->name, builtins[i]) == 0);
        const CHiderRegistry::SLookup byName = registry.find(HIDER_DOUBLE_NAME);
        REG_CHECK(byName.table != nullptr && byName.alias == nullptr);
        REG_CHECK(byName.table != nullptr && byName.table->dispatch == OR_HIDER_DISPATCH_BUCKETS);
        const CHiderRegistry::SLookup byAlias = registry.find(HIDER_DOUBLE_ALIAS);
        REG_CHECK(byAlias.table == byName.table && byAlias.alias != nullptr && byAlias.alias->deprecationWarning == nullptr);
        const CHiderRegistry::SLookup old = registry.find(HIDER_DOUBLE_DEPRECATED_ALIAS);
        REG_CHECK(old.table == byName.table && old.alias != nullptr && old.alias->deprecationWarning != nullptr);
        REG_CHECK(registry.find(HIDER_DOUBLE_NULLCREATE_NAME).table != nullptr);
    }

    // 2. By name: built once per thread, buckets dispatched as declared, destroyed through the table.
    {
        const SSession s = renderWith(HIDER_DOUBLE_NAME, counts, "name");
        REG_CHECK_MSG(s.frames.errors() == 0 && s.frames.warnings() == 0, "got:\n%s", s.frames.text().c_str());
        REG_CHECK(s.lastError == RIE_NOERROR);
        REG_CHECK_MSG(counts.created == 2, "created %d", counts.created.load());
        REG_CHECK_MSG(counts.threadMask == 3, "threads seen mask %d (0 and 1)", counts.threadMask.load());
        REG_CHECK(counts.numThreadsSeen == 2);
        REG_CHECK_MSG(counts.peersSeen == 2, "every create saw the frame's peer group (count == numThreads)");
        REG_CHECK_MSG(counts.preDisplay == 1, "preDisplaySetup once per frame, got %d", counts.preDisplay.load());
        REG_CHECK_MSG(counts.buckets == 4, "a 32x32 frame with 16x16 buckets has 4 bucket jobs, got %d", counts.buckets.load());
        REG_CHECK_MSG(counts.destroyed == 2, "destroyed %d", counts.destroyed.load());
    }

    // 3. By the plain alias: the same hider, silent, the canonical name stored.
    {
        RiBegin(RI_NULL);
        sessionReports().clear();
        RiErrorHandler(sessionHandler);
        RiHider(const_cast<RtToken>(HIDER_DOUBLE_ALIAS), RI_NULL);
        REG_CHECK(std::strcmp(options()->hider, HIDER_DOUBLE_NAME) == 0);
        REG_CHECK(sessionReports().list.empty());
        RiHider(const_cast<RtToken>(HIDER_DOUBLE_DEPRECATED_ALIAS), RI_NULL);
        REG_CHECK(std::strcmp(options()->hider, HIDER_DOUBLE_NAME) == 0);
        REG_CHECK_MSG(sessionReports().count(HIDER_DOUBLE_DEPRECATION_WARNING) == 1, "got:\n%s", sessionReports().text().c_str());
        REG_CHECK(sessionReports().warnings() == 1 && sessionReports().errors() == 0);
        RiEnd();

        const SSession s = renderWith(HIDER_DOUBLE_ALIAS, counts, "alias");
        REG_CHECK_MSG(s.frames.list.empty(), "got:\n%s", s.frames.text().c_str());
        REG_CHECK(counts.created == 2 && counts.buckets == 4 && counts.destroyed == 2);

        const SSession t = renderWith(HIDER_DOUBLE_DEPRECATED_ALIAS, counts, "oldalias");
        REG_CHECK_MSG(t.frames.count(HIDER_DOUBLE_DEPRECATION_WARNING) == 1, "once per statement, got:\n%s", t.frames.text().c_str());
        REG_CHECK(t.frames.errors() == 0);
        REG_CHECK(counts.created == 2 && counts.buckets == 4 && counts.destroyed == 2);
    }

    return finish("Registry_TestDoubleHider");
}

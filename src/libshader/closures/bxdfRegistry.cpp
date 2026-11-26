/**
 * Project: openRender
 *
 * File: bxdfRegistry.cpp
 *
 * Description:
 *   The Bxdf registry (spec 035). See bxdfRegistry.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "bxdfRegistry.h"

#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "common/pluginAbi.h"

namespace openrender::shading {

    namespace {

        const uint32_t kSupportedMajors[] = {OR_BXDF_ABI_VERSION};
        const SPluginFamily kBxdfFamily = {"bxdf", "orBxdfAbiVersion", OR_BXDF_ABI_VERSION, kSupportedMajors, 1, true};

        class CBxdfRegistry {
            public:
                CBxdfRegistry() { registerBuiltins(); }

                bool add(const OrBxdfTable *table, const char *origin) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    return addLocked(table, origin);
                }

                SBxdfLookup find(const char *name) {
                    SBxdfLookup out;
                    if (name == nullptr)
                        return out;
                    if (std::strncmp(name, "sl:", 3) == 0 || std::strncmp(name, "osl:", 4) == 0) {
                        out.reason = BxdfLookupReason::PrefixReserved;
                        return out;
                    }
                    std::lock_guard<std::mutex> lock(mutex_);
                    const auto it = tables_.find(name);
                    if (it != tables_.end()) {
                        out.table = it->second;
                        out.reason = BxdfLookupReason::Found;
                        return out;
                    }
                    const auto refused = refusedMajor_.find(name);
                    if (refused != refusedMajor_.end()) {
                        out.reason = BxdfLookupReason::VersionRefused;
                        out.major = refused->second;
                        return out;
                    }
                    out.reason = BxdfLookupReason::Unknown;
                    return out;
                }

            private:
                void registerBuiltins() {
                    for (const OrBxdfTable *t : {diffuseBxdfTable(), plasticBxdfTable(), conductorBxdfTable(), dielectricBxdfTable()})
                        addLocked(t, "built-in Bxdf");
                }

                bool addLocked(const OrBxdfTable *table, const char *origin) {
                    const char *shown = origin != nullptr ? origin : "(unknown origin)";
                    if (table == nullptr || table->name == nullptr || table->name[0] == '\0') {
                        error(CODE_BADTOKEN, "Bxdf registration without a name ignored (%s)\n", shown);
                        return false;
                    }
                    // One version check (gotcha 27): the table's own version through the declared-version overload, then the size rule.
                    const SPluginVersionVerdict verdict = checkPluginVersion(kBxdfFamily, table->header.abiVersion, shown);
                    if (verdict.verdict == EPluginVerdict::Reject) {
                        refusedMajor_[table->name] = OR_ABI_MAJOR(table->header.abiVersion); // a later statement names the reason
                        return false;
                    }
                    if (!checkPluginTable(kBxdfFamily, shown, &table->header, sizeof(OrBxdfTable), table->header.abiVersion))
                        return false;
                    if (tables_.count(table->name) != 0) {
                        error(CODE_BADTOKEN, "Bxdf \"%s\" is already registered; the second registration (%s) is ignored\n", table->name, shown);
                        return false;
                    }
                    tables_[table->name] = table;
                    return true;
                }

                std::mutex mutex_;
                std::map<std::string, const OrBxdfTable *> tables_;
                std::map<std::string, uint32_t> refusedMajor_;
        };

        CBxdfRegistry &registry() {
            static CBxdfRegistry *r = new CBxdfRegistry; // built on first use, never destroyed (late users around exit)
            return *r;
        }

    } // namespace

    bool registerBxdf(const OrBxdfTable *table, const char *origin) {
        return registry().add(table, origin);
    }

    SBxdfLookup findBxdf(const char *name) {
        return registry().find(name);
    }

} // namespace openrender::shading

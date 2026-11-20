/**
 * Project: openRender
 *
 * File: irradianceBatch.h
 *
 * Description:
 *   The batch scope of the irradiance cache step (spec 040, data-model.md section 4). A shading batch is what one shade() call evaluates (a REYES
 *   or zbuffer grid, a raytrace shading group). The records its GI lookups make are visible to the later lookups of the same batch and to nothing
 *   else until the frame's commit, with a radius clamped against the fixed part and against each other, so that what a point finds never depends
 *   on the order the batches ran in.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef IRRADIANCEBATCH_H
#define IRRADIANCEBATCH_H

#include <math.h>

#include <vector>

#include "common/algebra.h"
#include "ri/texture/irradianceStore.h"

namespace libshader {

    // One record of a batch for the clamp: its point, its raw radius (what the sampler returned) and its batch-clamped radius
    struct IrradianceBatchEntry {
            float P[3];
            float raw;
            float radius;
    };

    // The batch-clamped radius of a record about to join a batch, and the update of the records already in it (data-model.md section 4):
    //   new radius      = min(vsFixed, min over the earlier records j (raw_j + l))      (vsFixed = min(raw, min over F (dP + l)): clampAgainstFixed())
    //   earlier record j = min(radius_j, raw + l)
    // with l the distance between the points, C_EPSILON for coincident points. Every term is a raw or fixed radius plus a symmetric distance, so the
    // final radii do not depend on the order the records were added in. `batch` holds the earlier records of this cache; `count` of them.
    inline float clampAgainstBatch(IrradianceBatchEntry *batch, int count, const float *P, float raw, float vsFixed) {
        float radius = vsFixed;
        for (int j = 0; j < count; j++) {
            const float dx = batch[j].P[0] - P[0], dy = batch[j].P[1] - P[1], dz = batch[j].P[2] - P[2];
            float l = sqrtf(dx * dx + dy * dy + dz * dz);
            if (!(l > 0)) {
                l = (float)C_EPSILON;
            }
            const float fromEarlier = batch[j].raw + l;
            if (fromEarlier < radius) {
                radius = fromEarlier;
            }
            const float forEarlier = raw + l;
            if (forEarlier < batch[j].radius) {
                batch[j].radius = forEarlier;
            }
        }
        return radius;
    }

    // The records of the batch in progress, one list per cache, in creation order (the order the shader runs its GI calls, then lane order within a
    // call). Lives in a CShadingState and is reset at the start of every shading call.
    class IrradianceBatch {
        public:
            void reset() {
                for (PerCache &c : caches) {
                    c.entries.clear();
                    c.records.clear();
                }
            }

            // The records made so far for `cache`, each with its batch-clamped radius in dP: what lookupCached() reads after the fixed part
            CIrradianceBatchView view(const void *cache) const {
                for (const PerCache &c : caches) {
                    if (c.cache == cache) {
                        return CIrradianceBatchView{c.records.empty() ? nullptr : c.records.data(), (int)c.records.size()};
                    }
                }
                return CIrradianceBatchView{nullptr, 0};
            }

            // Add a record of `cache` (its dP the raw radius, `vsFixed` its radius against the fixed part)
            void add(const void *cache, const CIrradianceRecord &record, float vsFixed) {
                PerCache *list = nullptr;
                for (PerCache &c : caches) {
                    if (c.cache == cache) {
                        list = &c;
                        break;
                    }
                }
                if (list == nullptr) {
                    caches.push_back(PerCache());
                    list = &caches.back();
                    list->cache = cache;
                }
                const float radius = clampAgainstBatch(list->entries.data(), (int)list->entries.size(), record.P, record.dP, vsFixed);
                IrradianceBatchEntry e;
                e.P[0] = record.P[0];
                e.P[1] = record.P[1];
                e.P[2] = record.P[2];
                e.raw = record.dP;
                e.radius = radius;
                list->entries.push_back(e);
                list->records.push_back(record);
                // the earlier records' radii may have changed
                for (size_t j = 0; j < list->entries.size(); j++) {
                    list->records[j].dP = list->entries[j].radius;
                }
            }

        private:
            struct PerCache {
                    const void *cache = nullptr;
                    std::vector<IrradianceBatchEntry> entries;
                    std::vector<CIrradianceRecord> records;
            };
            std::vector<PerCache> caches;
    };

} // namespace libshader

#endif

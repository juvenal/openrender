/**
 * tests/gi_determinism/gi_recording.h
 *
 * Spec 040: the recording cache of spec 032's tests/gi_split/test_gi_keys.cpp (CRecordingCache and CRecordingServices), moved here so that the
 * tests of this directory share one copy (the spec 032 file is not edited). A render's irradiance cache is created and freed by the renderer
 * inside the frame, so the test installs a services wrapper over the renderer's own for the length of the render: its getCache() builds the
 * cache itself, as a CIrradianceCache subclass whose record hook copies every (key, record) before forwarding, and owns it.
 *
 * The record hook is virtual addPending() (spec 040, Phase 5; it was insertRecord() on the starting tree, and a CMake variable selected the form
 * while the phases were open). The lookupCached() the helper visitedByLookup() calls is the one with an accept function and a batch view.
 */

#ifndef GI_RECORDING_H
#define GI_RECORDING_H

#include <algorithm>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "ri.h"
#include "libshader/shading/shading.h"
#include "ri/render/renderer.h"
#include "ri/texture/irradianceCache.h"
#include "shading_contract/contract_forwarder.h"

#include "gi_detkit.h"

namespace gidet {

    struct SeenRecord {
            uint64_t key;
            std::string bits; // the 56 floats of the record in file order, as hex
    };

    inline std::string hexKey(uint64_t key) {
        char b[24];
        snprintf(b, sizeof(b), "%016llx", (unsigned long long)key);
        return b;
    }

    inline std::string recordFileBits(const CIrradianceRecord &r) {
        float f[56];
        memcpy(f, r.P, 12);
        memcpy(f + 3, r.N, 12);
        memcpy(f + 6, r.irradiance, 12);
        f[9] = r.coverage;
        memcpy(f + 10, r.envdir, 12);
        memcpy(f + 13, r.gP, 84);
        memcpy(f + 34, r.gR, 84);
        f[55] = r.dP;
        return bits(f, 56);
    }

    // An irradiance cache that copies every record the sampler hands it, then forwards it
    class RecordingCache final : public CIrradianceCache {
        public:
            using CIrradianceCache::CIrradianceCache;

            void addPending(const CIrradianceRecord &r, float maxError) override {
                note(r);
                CIrradianceCache::addPending(r, maxError);
            }
            std::vector<SeenRecord> seen() {
                std::lock_guard<std::mutex> g(m);
                return list;
            }

        private:
            void note(const CIrradianceRecord &r) {
                std::lock_guard<std::mutex> g(m);
                list.push_back({r.key, recordFileBits(r)});
            }
            std::mutex m;
            std::vector<SeenRecord> list;
    };

    // The renderer's services with getCache() replaced (CContractForwarder forwards every other member)
    class RecordingServices final : public CContractForwarder {
        public:
            explicit RecordingServices(CRendererServices *inner, unsigned flags) : CContractForwarder(inner), flags(flags) {}
            ~RecordingServices() override { delete cache; }

            RecordingCache *cache = nullptr;

            CTexture3d *getCache(const char *handle, const char *mode, const float *from, const float *to) override {
                if (cache == nullptr) {
                    (void)mode;
                    cache = new RecordingCache(handle != nullptr ? handle : "", flags, nullptr, from, to, CRenderer::toNDC, CRenderer::worldBmin, CRenderer::worldBmax);
                }
                return cache;
            }

        private:
            unsigned flags;
    };

    // Fill a cache file with `count` records, all of which a lookup at the origin visits and none of which it accepts: the records lie in
    // the plane z = 0 (so the front test passes), face +z while the probe surface faces -z (the directional error 11.5 gives a negative
    // weight), and carry a radius of 3 (so they sit in the root node, which every lookup visits)
    inline bool writeRejectedRecords(const std::string &name, int count) {
        remove(name.c_str());
        matrix from, to;
        identitym(from);
        identitym(to);
        const float bmin[3] = {-1, -1, -1}, bmax[3] = {1, 1, 1};
        {
            CIrradianceCache cache(name.c_str(), CACHE_WRITE | CACHE_SAMPLE, NULL, from, to, NULL, bmin, bmax);
            Lcg g(1234);
            for (int i = 0; i < count; i++) {
                CIrradianceRecord r;
                memset(&r, 0, sizeof(r));
                const float angle = g.range(0, 6.2831853f), radius = g.range(0.8f, 2.0f);
                r.P[0] = radius * cosf(angle);
                r.P[1] = radius * sinf(angle);
                r.P[2] = 0;
                r.N[2] = 1;
                r.irradiance[0] = r.irradiance[1] = r.irradiance[2] = g.range(0.1f, 1);
                r.coverage = g.range(0, 1);
                r.envdir[2] = 1;
                r.dP = 3.0f;
                r.key = 0;
                cache.addPending(r, 0.6f);
            }
            cache.commitPending();
        }
        Bytes b;
        return gitest::readFile(name, b) && b.size() > 40;
    }

    // How many records a lookup at the origin of the probe surface (normal -z) visits (is asked an accept value for), and whether it hit
    namespace detail {
        struct Counter {
                mutable int n = 0;
        };
        inline float countAccept(const void *s, uint64_t) {
            static_cast<const Counter *>(s)->n++;
            return 0.5f;
        }
    } // namespace detail

    inline int visitedByLookup(const std::string &file, bool &hit) {
        matrix from, to;
        identitym(from);
        identitym(to);
        const float bmin[3] = {-1, -1, -1}, bmax[3] = {1, 1, 1};
        CIrradianceCache cache(file.c_str(), CACHE_READ | CACHE_RDONLY | CACHE_SAMPLE, NULL, from, to, NULL, bmin, bmax);
        const float P[3] = {0, 0, 0}, N[3] = {0, 0, -1};
        float C[7], Pc[3], Nc[3];
        detail::Counter counter;
        const CIrradianceBatchView noBatch{nullptr, 0};
        hit = cache.lookupCached(C, P, N, 0.6f, detail::countAccept, &counter, noBatch, Pc, Nc);
        return counter.n;
    }

} // namespace gidet

#endif // GI_RECORDING_H

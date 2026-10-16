// Sampling_Range (G2) and Sampling_Purity (G1).
#include <algorithm>
#include <array>
#include <atomic>
#include <random>
#include <thread>

#include "sampling_test_util.h"

using namespace sptest;

namespace {

struct Triple {
    sp::SubstreamKey key;
    uint32_t dim, index;
};

// Deterministic (key, dimension, index) triples. One in eight uses a large
// index, one in eight a large dimension.
Triple triple(uint64_t t) {
    const uint64_t h = splitmix(t * 0x9E3779B97F4A7C15ull + 12345u);
    Triple r;
    r.key = rawKey(h);
    r.dim = (t % 8 == 1) ? static_cast<uint32_t>((h >> 20) % (1u << 20)) : static_cast<uint32_t>((h >> 8) % 64);
    r.index = (t % 8 == 0) ? static_cast<uint32_t>((h >> 32) % (1u << 24)) : static_cast<uint32_t>((h >> 40) % 4096);
    return r;
}

struct Result {
    uint32_t a, b, c;  // bits of get1D, get2D.u, get2D.v
    bool operator==(const Result &o) const { return a == o.a && b == o.b && c == o.c; }
};

Result evaluate(sp::Backend be, const Triple &t) {
    const float one = sp::get1D(be, t.key, t.dim, t.index);
    const sp::Sample2D two = sp::get2D(be, t.key, t.dim, t.index);
    return {bitsOf(one), bitsOf(two.u), bitsOf(two.v)};
}

void range() {
    constexpr uint64_t kDraws = 1u << 18;
    for (sp::Backend be : kBackends) {
        for (uint64_t t = 0; t < kDraws; ++t) {
            const Triple tr = triple(t);
            const float one = sp::get1D(be, tr.key, tr.dim, tr.index);
            const sp::Sample2D two = sp::get2D(be, tr.key, tr.dim, tr.index);
            if (!inRange(one) || !inRange(two.u) || !inRange(two.v)) {
                std::fprintf(stderr, "%s: out of range at t=%llu: %.9g %.9g %.9g\n", backendName(be),
                             static_cast<unsigned long long>(t), one, two.u, two.v);
                ++failureCount();
                break;
            }
        }
        // Edges of the index and dimension ranges.
        for (uint32_t dim : {0u, 1u, 63u, (1u << 20) - 1u, 1u << 20}) {
            for (uint32_t index : {0u, 1u, 4095u, 4096u, (1u << 24) - 1u, 1u << 24}) {
                const sp::Sample2D v = sp::get2D(be, rawKey(7), dim, index);
                SP_CHECK(inRange(v.u) && inRange(v.v));
                SP_CHECK(inRange(sp::get1D(be, rawKey(7), dim, index)));
            }
        }
    }
}

void purity() {
    constexpr uint32_t kTriples = 1u << 16;
    constexpr unsigned kThreads = 8;
    for (sp::Backend be : kBackends) {
        // The threaded pass runs first, and starts all threads together, so that
        // any lazily built tables (PMJ02) are first touched concurrently.
        std::vector<std::vector<Result>> out(kThreads, std::vector<Result>(kTriples));
        std::atomic<bool> go{false};
        std::vector<std::thread> pool;
        for (unsigned th = 0; th < kThreads; ++th)
            pool.emplace_back([&, th] {
                std::vector<uint32_t> order(kTriples);
                for (uint32_t i = 0; i < kTriples; ++i) order[i] = i;
                std::mt19937 rng(1234u + th);
                std::shuffle(order.begin(), order.end(), rng);
                while (!go.load(std::memory_order_acquire)) {
                }
                for (uint32_t i : order) out[th][i] = evaluate(be, triple(i));
            });
        go.store(true, std::memory_order_release);
        for (auto &t : pool) t.join();

        std::vector<Result> serial(kTriples);
        for (uint32_t i = 0; i < kTriples; ++i) serial[i] = evaluate(be, triple(i));

        for (unsigned th = 0; th < kThreads; ++th) {
            uint32_t bad = 0;
            for (uint32_t i = 0; i < kTriples; ++i) bad += !(out[th][i] == serial[i]);
            if (bad) {
                std::fprintf(stderr, "%s: thread %u differs from the serial pass in %u of %u triples\n",
                             backendName(be), th, bad, kTriples);
                ++failureCount();
            }
        }
    }
}

}  // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv, {{"range", range}, {"purity", purity}});
}

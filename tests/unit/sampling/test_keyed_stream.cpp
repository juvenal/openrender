// Sampling_KeyedStream: CKeyedStream, the local stream for randomness that is
// not a sample (clustering, getDisp, the point hierarchy; research R3.10). It
// hands out hash(key, counter++), so it must be a pure function of its key.
#include <algorithm>
#include <atomic>
#include <set>
#include <thread>

#include "sampling_test_util.h"

using namespace sptest;

namespace {

// One draw of either kind; the pattern of kinds is part of the sequence.
struct Draw {
    bool isFloat;
    uint32_t bits;  // float bits for next1D, the value for nextU32
    bool operator==(const Draw &o) const { return isFloat == o.isFloat && bits == o.bits; }
};

std::vector<Draw> sequence(uint64_t key, uint32_t n) {
    sp::CKeyedStream s(key);
    std::vector<Draw> out;
    for (uint32_t i = 0; i < n; ++i) {
        if (i % 3 == 2)
            out.push_back({false, s.nextU32()});
        else
            out.push_back({true, bitsOf(s.next1D())});
    }
    return out;
}

std::vector<uint32_t> u32s(uint64_t key, uint32_t n) {
    sp::CKeyedStream s(key);
    std::vector<uint32_t> out(n);
    for (auto &v : out) v = s.nextU32();
    return out;
}

std::vector<float> floats(uint64_t key, uint32_t n) {
    sp::CKeyedStream s(key);
    std::vector<float> out(n);
    for (auto &v : out) v = s.next1D();
    return out;
}

double correlation(const std::vector<float> &a, const std::vector<float> &b) {
    const size_t n = a.size();
    double ma = 0, mb = 0;
    for (size_t i = 0; i < n; ++i) ma += a[i] / n, mb += b[i] / n;
    double sab = 0, saa = 0, sbb = 0;
    for (size_t i = 0; i < n; ++i) {
        sab += (a[i] - ma) * (b[i] - mb);
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
    }
    return sab / std::sqrt(saa * sbb);
}

void keyedStream() {
    constexpr uint32_t kN = 1u << 14;

    // The same key gives an identical sequence, whatever the mix of calls.
    for (uint64_t key : {0ull, 1ull, 0xDEADBEEFCAFEF00Dull, ~0ull}) {
        SP_CHECK(sequence(key, 4096) == sequence(key, 4096));
        // A shorter run is a prefix of a longer one: the counter is the only state.
        const std::vector<Draw> shortRun = sequence(key, 100), longRun = sequence(key, 4096);
        SP_CHECK(std::equal(shortRun.begin(), shortRun.end(), longRun.begin()));
    }

    // next1D stays in [0, 1 - 2^-24] (never 1.0); both draw kinds actually vary.
    const std::vector<float> f = floats(42, 1u << 16);
    double mean = 0.0;
    for (float v : f) {
        SP_CHECK(inRange(v));
        mean += v / f.size();
    }
    SP_CHECK(std::fabs(mean - 0.5) < 5.0 * 0.28868 / std::sqrt(static_cast<double>(f.size())));

    // nextU32 spans the full 32-bit range: every bit is set about half the time
    // (five standard deviations of a binomial with n = 2^16).
    const std::vector<uint32_t> u = u32s(42, 1u << 16);
    const double tol = 5.0 * 0.5 * std::sqrt(static_cast<double>(u.size()));
    for (int bit = 0; bit < 32; ++bit) {
        uint32_t ones = 0;
        for (uint32_t v : u) ones += (v >> bit) & 1u;
        SP_CHECK(std::fabs(static_cast<double>(ones) - 0.5 * u.size()) < tol);
    }

    // Different keys give different streams: no equal value at the same
    // position, and no linear correlation, for adjacent keys and for keys that
    // differ in a single high bit.
    const std::pair<uint64_t, uint64_t> pairs[] = {
        {100, 101}, {0, 1}, {0xFFFFull, 0x10000ull}, {7, 7 ^ (1ull << 63)}, {7, 7 ^ (1ull << 32)}};
    for (const auto &p : pairs) {
        const std::vector<uint32_t> a = u32s(p.first, kN), b = u32s(p.second, kN);
        uint32_t equal = 0;
        for (uint32_t i = 0; i < kN; ++i) equal += a[i] == b[i];
        SP_CHECK(equal == 0);
        const double r = correlation(floats(p.first, kN), floats(p.second, kN));
        // Five standard deviations of the correlation of independent streams.
        SP_CHECK(std::fabs(r) < 5.0 / std::sqrt(static_cast<double>(kN)));
    }
    // A stream is not a shifted copy of the stream of the next key.
    {
        const std::vector<uint32_t> a = u32s(5, 64), b = u32s(6, 64);
        std::set<uint32_t> sa(a.begin(), a.end());
        uint32_t shared = 0;
        for (uint32_t v : b) shared += sa.count(v);
        SP_CHECK(shared == 0);
    }

    // Thread-independent: streams built concurrently match the serial ones.
    constexpr unsigned kThreads = 8;
    std::vector<std::vector<Draw>> serial(4), threaded(kThreads);
    for (unsigned k = 0; k < 4; ++k) serial[k] = sequence(1000 + k, 8192);
    std::atomic<bool> go{false};
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < kThreads; ++t)
        pool.emplace_back([&, t] {
            while (!go.load(std::memory_order_acquire)) {
            }
            threaded[t] = sequence(1000 + t % 4, 8192);
        });
    go.store(true, std::memory_order_release);
    for (auto &th : pool) th.join();
    for (unsigned t = 0; t < kThreads; ++t) SP_CHECK(threaded[t] == serial[t % 4]);
}

}  // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv, {{"keyed_stream", keyedStream}});
}

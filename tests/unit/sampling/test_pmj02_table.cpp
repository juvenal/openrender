// Sampling_PMJ02Construction: the PMJ02 patterns are progressive multi-jittered
// (0,2) sequences in the sense of Christensen, Kensler and Kilpatrick 2018, and
// not a randomised Sobol (0,2)-sequence.
//
// Every (0,2)-sequence has (0,m,2)-net prefixes, Owen-scrambled Sobol included,
// so the net property alone cannot tell the two apart (Sampling_PMJ02Property
// checks it). What does tell them apart is the paper's generation strategy:
// the sequence is built by doubling, and each new sample is generated inside
// the stratum of the sample it is generated from. At N = 4^j that stratum is
// one cell of the sqrt(N) x sqrt(N) grid, and the new sample goes in the
// *diagonally opposite* sub-quadrant of it ("a simple, diagonally alternating
// strategy that progressively fills in holes in increasingly fine
// stratifications"). The checks, on the table points in sequence order, for
// N = 2^k with even k (N = 4^j, a square cell grid):
//   - locality: sample N + i lies in the same j-digit cell as sample i;
//   - diagonal: sample N + i is in the opposite half of that cell in both x and y.
// A Sobol sequence breaks both: sample N + i has its first y digit flipped
// relative to sample i. The test applies the same checker to Sobol points and
// requires it to report violations, so a checker that accepts everything
// cannot pass.
//
// The odd-k steps (N = 2 * 4^j) are not checked. Pharr 2019 and the paper's
// abstract support the even-k diagonal rule, but this test has not been
// confirmed against the paper's text for the odd step, where the new sample is
// understood to go in the same cell in an adjacent sub-quadrant. The odd-k
// locality count is printed for information only; enable it as a check once
// someone has confirmed the rule from Christensen et al. 2018.
#include <atomic>
#include <set>
#include <thread>

#include "sampling_test_util.h"

using namespace sptest;

namespace {

    constexpr uint32_t kPoints = sp::detail::kPmj02Points;

    // The first `digits` binary digits of u (exact: u has 24 bits).
    uint32_t digits(float u, int count) {
        return count == 0 ? 0u : static_cast<uint32_t>(std::floor(u * static_cast<float>(1u << count)));
    }

    struct Report {
            int net = 0, locality = 0, diagonal = 0, range = 0;
            int localityOdd = 0; // informational only, see the file comment
    };

    Report check(const std::vector<sp::Sample2D> &pts) {
        Report r;
        for (const auto &p : pts)
            if (!inRange(p.u) || !inRange(p.v))
                ++r.range;

        // Every power-of-two prefix is a (0, m, 2)-net.
        for (int m = 1; m <= 12; ++m) {
            const uint32_t n = 1u << m;
            for (int a = 0; a <= m; ++a) {
                const int b = m - a;
                std::vector<uint8_t> count(n, 0);
                bool ok = true;
                for (uint32_t i = 0; i < n; ++i) {
                    uint8_t &c = count[(digits(pts[i].u, a) << b) | digits(pts[i].v, b)];
                    if (c++)
                        ok = false;
                }
                if (!ok)
                    ++r.net;
            }
        }

        for (int k = 0; k < 12; ++k) {
            const uint32_t n = 1u << k;
            const int j = k / 2;
            for (uint32_t i = 0; i < n; ++i) {
                const sp::Sample2D &p = pts[i], &q = pts[n + i];
                const bool sameCell = digits(p.u, j) == digits(q.u, j) && digits(p.v, j) == digits(q.v, j);
                if (k % 2 != 0) {
                    if (!sameCell)
                        ++r.localityOdd;
                    continue;
                }
                if (!sameCell)
                    ++r.locality;
                if (digits(p.u, j + 1) == digits(q.u, j + 1) || digits(p.v, j + 1) == digits(q.v, j + 1))
                    ++r.diagonal;
            }
        }
        return r;
    }

    std::vector<sp::Sample2D> pattern(uint32_t p) {
        std::vector<sp::Sample2D> pts(kPoints);
        for (uint32_t j = 0; j < kPoints; ++j)
            pts[j] = sp::detail::pmj02Point(p, j);
        return pts;
    }

    void construction() {
        // The tables are built once, on first use: touch them from 8 threads at the
        // same time, and compare with a later serial read.
        std::vector<std::vector<sp::Sample2D>> first(8);
        {
            std::atomic<bool> go{false};
            std::vector<std::thread> pool;
            for (unsigned t = 0; t < 8; ++t)
                pool.emplace_back([&, t] {
                    while (!go.load(std::memory_order_acquire)) {
                    }
                    first[t] = pattern(t * 7u);
                });
            go.store(true, std::memory_order_release);
            for (auto &th : pool)
                th.join();
        }

        std::set<std::pair<uint32_t, uint32_t>> starts;
        int totalLocality = 0, totalDiagonal = 0, totalLocalityOdd = 0;
        for (uint32_t p = 0; p < sp::detail::kPmj02Patterns; ++p) {
            const std::vector<sp::Sample2D> pts = pattern(p);
            const Report r = check(pts);
            if (r.range || r.net || r.locality || r.diagonal)
                std::fprintf(stderr, "pattern %u: range %d, net %d, locality %d, diagonal %d violation(s)\n", p, r.range,
                             r.net, r.locality, r.diagonal);
            SP_CHECK(r.range == 0 && r.net == 0 && r.locality == 0 && r.diagonal == 0);
            totalLocality += r.locality;
            totalLocalityOdd += r.localityOdd;
            totalDiagonal += r.diagonal;

            // Deterministic: a second read gives the same bits, as does the threaded first touch.
            const std::vector<sp::Sample2D> again = pattern(p);
            bool same = true;
            for (uint32_t j = 0; j < kPoints; ++j)
                same = same && bitsOf(pts[j].u) == bitsOf(again[j].u) && bitsOf(pts[j].v) == bitsOf(again[j].v);
            SP_CHECK(same);
            if (p % 7 == 0 && p / 7 < 8) {
                const auto &f = first[p / 7];
                bool match = true;
                for (uint32_t j = 0; j < kPoints; ++j)
                    match = match && bitsOf(pts[j].u) == bitsOf(f[j].u) && bitsOf(pts[j].v) == bitsOf(f[j].v);
                SP_CHECK(match);
            }
            starts.insert({bitsOf(pts[0].u), bitsOf(pts[0].v)});
        }
        // The 64 patterns are distinct.
        SP_CHECK(starts.size() == sp::detail::kPmj02Patterns);

        // Negative control: Sobol (scrambled or not) fails the locality and
        // diagonal checks, so the checker discriminates. The net property alone
        // would pass both.
        std::vector<sp::Sample2D> unscrambled(kPoints), scrambled(kPoints);
        for (uint32_t i = 0; i < kPoints; ++i) {
            unscrambled[i] = sp::detail::sobolUnscrambled(i);
            scrambled[i] = sp::get2D(sp::Backend::Sobol, rawKey(3), 0, i);
        }
        for (const auto *pts : {&unscrambled, &scrambled}) {
            const Report r = check(*pts);
            std::printf("Sobol control: net %d, locality %d, diagonal %d violation(s) (odd-k locality, informational: %d)\n", r.net, r.locality, r.diagonal, r.localityOdd);
            SP_CHECK(r.net == 0);     // it is a (0,2)-sequence
            SP_CHECK(r.locality > 0); // but not a PMJ02 construction
            SP_CHECK(r.diagonal > 0);
        }
        std::printf("PMJ02: %u patterns, %d locality and %d diagonal violations (odd-k locality, informational: %d)\n", sp::detail::kPmj02Patterns, totalLocality, totalDiagonal, totalLocalityOdd);
    }

} // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv, {{"construction", construction}});
}

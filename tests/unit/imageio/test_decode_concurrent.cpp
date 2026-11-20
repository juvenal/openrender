// tests/unit/imageio/test_decode_concurrent.cpp -- ImageIO_DecodeConcurrent (spec 048, T058, gotcha 25).
//
// Several threads each hold several open OIIO inputs at once and read different fixtures; every result equals the
// legacy decode of the same file, made first on the main thread. A corrupted file on one thread produces an error()
// report and no exception. The test runs with no CTextureSystem: nothing in the decode path may need the texture
// system's load lock.
#include "decode_harness.h"

#include <atomic>
#include <thread>

using namespace harness;

int main() {
    installSink();

    std::vector<const char *> suffixes = {".tif", ".png", ".hdr"};
#ifdef HAVE_OPENEXR
    suffixes.push_back(".exr");
#endif
    std::vector<std::string> all = listFiles(imageioFixtures() + "/parity", suffixes);
    EXPECT(all.size() >= 12, "only %zu parity fixtures", all.size());

    // the reference: the legacy decode of every fixture, single-threaded
    std::vector<Decoded> reference;
    for (const std::string &p : all) {
        reference.push_back(decode(p, EImageBackend::Legacy));
        EXPECT(reference.back().ok(), "legacy refused %s", p.c_str());
    }

    // a corrupted copy of a PNG for the failing thread
    std::vector<uint8_t> bad = readFile(legacyFixtures() + "/large_rgb8.png");
    for (size_t i = 0; i < 16 && i < bad.size(); i++)
        bad[i] = 0xAA;
    const std::string corrupted = scratchDir() + "/concurrent_corrupted.png";
    EXPECT(writeFile(corrupted, bad), "cannot write %s", corrupted.c_str());

    const int threads = 4, perThread = 3, rounds = 20;
    std::atomic<int> mismatches{0}, exceptions{0}, corruptedAccepted{0}, corruptedSilent{0};
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; t++)
        pool.emplace_back([&, t] {
            try {
                for (int r = 0; r < rounds; r++) {
                    // several inputs open at once
                    struct Open {
                            CImageInput *in = nullptr;
                            CImageInfo info;
                            size_t index = 0;
                    };
                    std::vector<Open> open(perThread);
                    for (int k = 0; k < perThread; k++) {
                        open[k].index = (size_t)((t * perThread + k + r) % (int)all.size());
                        open[k].in = createImageInput(all[open[k].index].c_str(), EImageBackend::OIIO);
                        if (open[k].in == nullptr || !open[k].in->open(all[open[k].index].c_str(), open[k].info))
                            mismatches++;
                    }
                    for (int k = 0; k < perThread; k++) {
                        if (open[k].in == nullptr)
                            continue;
                        Decoded got;
                        got.info = open[k].info;
                        got.bytes.assign(byteCount(got.info), 0xA5);
                        got.created = got.opened = true;
                        got.read = open[k].in->readImage(got.bytes.data());
                        std::string why;
                        if (!got.read || !sameSamples(reference[open[k].index], got, why))
                            mismatches++;
                        open[k].in->close();
                        delete open[k].in;
                    }
                    // one thread also decodes the corrupted file, and must see a report and no exception
                    if (t == 0 && r % 5 == 0) {
                        Decoded d = decode(corrupted, EImageBackend::OIIO);
                        if (d.created && d.opened && d.read)
                            corruptedAccepted++;
                        if (d.messages.empty())
                            corruptedSilent++;
                    }
                }
            } catch (...) {
                exceptions++;
            }
        });
    for (std::thread &th : pool)
        th.join();

    EXPECT(mismatches == 0, "%d concurrent decodes differ from the legacy decode", mismatches.load());
    EXPECT(exceptions == 0, "%d exception(s) escaped a thread", exceptions.load());
    EXPECT(corruptedAccepted == 0, "the corrupted file was accepted %d time(s)", corruptedAccepted.load());
    EXPECT(corruptedSilent == 0, "the corrupted file failed without a report %d time(s)", corruptedSilent.load());
    {
        std::lock_guard<std::mutex> lock(sink().mutex);
        EXPECT(sink().orphans.empty(), "%zu report(s) arrived from a thread that had not asked for capture", sink().orphans.size());
    }
    printf("ImageIO_DecodeConcurrent: %d threads x %d inputs x %d rounds\n", threads, perThread, rounds);
    return finish("ImageIO_DecodeConcurrent");
}

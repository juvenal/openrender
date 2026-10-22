// Sampling_Keys (data-model: SampleKey, SubstreamKey).
#include "sampling_test_util.h"

using namespace sptest;

namespace {

sp::SampleId idWith(uint32_t sampleIndex) { return sp::SampleId{10, -4, sampleIndex, 3u, 0xFFFFFFFEu}; }

void keys() {
    // Camera keys: path never changes with the camera sampleIndex; full does.
    const sp::SampleKey a = sp::cameraKey(idWith(2));
    const sp::SampleKey b = sp::cameraKey(idWith(7));
    SP_CHECK(a.path == b.path);
    SP_CHECK(a.full != b.full);
    SP_CHECK(a.pixelSampleIndex == 2 && b.pixelSampleIndex == 7);
    SP_CHECK(sp::cameraKey(idWith(2)).full == a.full);  // pure

    // Pixel, frame and seed enter both hashes.
    sp::SampleId px = idWith(2);
    px.px += 1;
    SP_CHECK(sp::cameraKey(px).path != a.path && sp::cameraKey(px).full != a.full);
    sp::SampleId py = idWith(2);
    py.py += 1;
    SP_CHECK(sp::cameraKey(py).path != a.path);
    sp::SampleId fr = idWith(2);
    fr.frame += 1;
    SP_CHECK(sp::cameraKey(fr).path != a.path);
    sp::SampleId sd = idWith(2);
    sd.seed += 1;
    SP_CHECK(sp::cameraKey(sd).path != a.path);

    // Cell keys: path == full, no pixel sample.
    const sp::SampleKey c = sp::cellKey(0x1234, 0.25f, 0.75f, 5u, 3u, 9u);
    SP_CHECK(c.path == c.full);
    SP_CHECK(c.pixelSampleIndex == -1);
    SP_CHECK(sp::cellKey(0x1234, 0.25f, 0.75f, 5u, 3u, 9u).full == c.full);  // pure
    SP_CHECK(sp::cellKey(0x1235, 0.25f, 0.75f, 5u, 3u, 9u).full != c.full);  // stableId
    SP_CHECK(sp::cellKey(0x1234, 0.5f, 0.75f, 5u, 3u, 9u).full != c.full);   // u
    SP_CHECK(sp::cellKey(0x1234, 0.25f, 0.5f, 5u, 3u, 9u).full != c.full);   // v
    SP_CHECK(sp::cellKey(0x1234, 0.25f, 0.75f, 6u, 3u, 9u).full != c.full);  // elementId
    SP_CHECK(sp::cellKey(0x1234, 0.25f, 0.75f, 5u, 4u, 9u).full != c.full);  // frame
    SP_CHECK(sp::cellKey(0x1234, 0.25f, 0.75f, 5u, 3u, 10u).full != c.full); // seed

    // childKey derives both hashes and keeps the camera sample index.
    const sp::SampleKey ca = sp::childKey(a, 1, 4, 0, 0);
    const sp::SampleKey cb = sp::childKey(b, 1, 4, 0, 0);
    SP_CHECK(ca.path == cb.path);          // parents differ only in the camera sampleIndex
    SP_CHECK(ca.full != cb.full);
    SP_CHECK(ca.path != a.path && ca.full != a.full);
    SP_CHECK(ca.pixelSampleIndex == 2 && cb.pixelSampleIndex == 7);
    SP_CHECK(sp::childKey(a, 1, 4, 0, 0).full == ca.full);  // pure
    // Every argument enters both hashes.
    SP_CHECK(sp::childKey(a, 2, 4, 0, 0).path != ca.path && sp::childKey(a, 2, 4, 0, 0).full != ca.full);
    SP_CHECK(sp::childKey(a, 1, 5, 0, 0).path != ca.path && sp::childKey(a, 1, 5, 0, 0).full != ca.full);
    SP_CHECK(sp::childKey(a, 1, 4, 1, 0).path != ca.path && sp::childKey(a, 1, 4, 1, 0).full != ca.full);
    SP_CHECK(sp::childKey(a, 1, 4, 0, 1).path != ca.path && sp::childKey(a, 1, 4, 0, 1).full != ca.full);
    // A child of a cell key stays a cell-style key: no pixel sample, path == full.
    const sp::SampleKey cc = sp::childKey(c, 1, 4, 0, 0);
    SP_CHECK(cc.pixelSampleIndex == -1);
    SP_CHECK(cc.path == cc.full);

    // substream(): pixel-sample points seed from path and offset the index by
    // pixelSampleIndex * N; cell points seed from full with indexBase 0.
    constexpr uint32_t kN = 16;
    const sp::SubstreamKey sa = sp::substream(a, 99u, 3, 1, 0, kN);
    const sp::SubstreamKey sb = sp::substream(b, 99u, 3, 1, 0, kN);
    SP_CHECK(sa.seedHash == sb.seedHash);     // same path, different camera sample
    SP_CHECK(sa.indexBase == 2u * kN);
    SP_CHECK(sb.indexBase == 7u * kN);

    const sp::SubstreamKey sc = sp::substream(c, 99u, 3, 1, 0, kN);
    SP_CHECK(sc.indexBase == 0u);
    const sp::SampleKey c2 = sp::cellKey(0x1234, 0.25f, 0.75f, 6u, 3u, 9u);
    SP_CHECK(sp::substream(c2, 99u, 3, 1, 0, kN).seedHash != sc.seedHash);  // seeds from the cell key

    // The pixel-sample seed depends on path: a different pixel changes it.
    SP_CHECK(sp::substream(sp::cameraKey(px), 99u, 3, 1, 0, kN).seedHash != sa.seedHash);

    // Every substream argument matters.
    SP_CHECK(sp::substream(a, 100u, 3, 1, 0, kN).seedHash != sa.seedHash);  // shaderKey
    SP_CHECK(sp::substream(a, 99u, 4, 1, 0, kN).seedHash != sa.seedHash);   // callSite
    SP_CHECK(sp::substream(a, 99u, 3, 2, 0, kN).seedHash != sa.seedHash);   // occurrence
    SP_CHECK(sp::substream(a, 99u, 3, 1, 1, kN).seedHash != sa.seedHash);   // depth
    SP_CHECK(sp::substream(a, 99u, 3, 1, 0, kN).seedHash == sa.seedHash);   // pure
    // samplesPerCall scales the index base only.
    SP_CHECK(sp::substream(a, 99u, 3, 1, 0, 2u * kN).seedHash == sa.seedHash);
    SP_CHECK(sp::substream(a, 99u, 3, 1, 0, 2u * kN).indexBase == 4u * kN);
}

}  // namespace

int main(int argc, char **argv) {
    return runCase(argc, argv, {{"keys", keys}});
}

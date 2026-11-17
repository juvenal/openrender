// tests/unit/imageio/test_bake_parity.cpp -- ImageIO_BakeParity (spec 048, T088, SC-002, FR-015, FR-016, P-Q3, P-Q3a).
//
// Every case of bake_cases.h that OIIO writes (the allowed (bits, channels) of data-model section 4) is baked twice through the make*
// entries: by the legacy writer (no "imageio" parameter) and by the OIIO writer ("imageio" "oiio": the OIIO decoder and the OIIO
// writer). Both files are read with libtiff, directory by directory, and compared per the table of contracts/bake-writer.md:
// the Pixar tags (texture format, wrap modes, the two matrices, the full size on the first directory of every pyramid INCLUDING when it
// equals the data size, the field of view), the geometry and tiling, the sample layout, LZW with no predictor, the directory count,
// and every in-image sample of every tile. The OIIO bake carries a SOFTWARE tag that begins "OpenImageIO" and the legacy bake carries
// none: that is the probe that tells the backends apart (moved here from ImageIO_Selection, which has no writer to probe yet).
//
// A source that is resized (a non power of two source) is compared with the legacy bake made on the SAME platform only: the legacy
// bytes of such a bake differ between macOS and Linux, so no hash or file of the other platform is ever used here.
// First, a legacy-against-legacy self-check of the comparison (green on the starting tree by design).
#include "bake_cases.h"
#include "bake_compare.h"

using namespace sel;

int main() {
    RiBegin(RI_NULL);

    // ---- the comparison, on the legacy writer alone ----
    {
        const bakecases::BakeCase &c = bakecases::cases().front();
        const std::string one = scratch() + "/self-1.tex", two = scratch() + "/self-2.tex";
        bakecases::bakeCase(c, one, nullptr);
        bakecases::bakeCase(c, two, nullptr);
        int before = failures();
        int dirs = bakecmp::compare(one, two, "self-check");
        CHECK(failures() == before);
        CHECK(dirs > 1); // a real pyramid
        CHECK(!bakecmp::hasSoftware(one)); // the legacy writer writes no SOFTWARE tag
        printf("harness self-check (legacy against legacy): %d directories\n", dirs);
    }

    // ---- legacy writer against OIIO writer ----
    int compared = 0;
    for (const bakecases::BakeCase &c : bakecases::cases()) {
        if (c.excluded)
            continue; // ImageIO_BakeExclusions
        const std::string legacy = scratch() + "/legacy-" + c.name + ".tex";
        const std::string oiio = scratch() + "/oiio-" + c.name + ".tex";
        bakecases::bakeCase(c, legacy, nullptr);
        bakecases::bakeCase(c, oiio, "oiio");
        CHECK(exists(legacy));
        CHECK(exists(oiio));
        if (!exists(legacy) || !exists(oiio))
            continue;
        CHECK(!bakecmp::hasSoftware(legacy));
        CHECK(bakecmp::writtenByOiio(oiio));
        bakecmp::compare(legacy, oiio, c.name);
        compared++;
    }
    CHECK(compared == 28); // every case of bake_cases.h that is not excluded
    RiEnd();
    printf("ImageIO_BakeParity: %d bakes compared\n", compared);
    return finish("ImageIO_BakeParity");
}

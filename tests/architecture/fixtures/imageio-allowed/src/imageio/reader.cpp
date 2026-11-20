#include "common/types.h"
#include "imageio/orImageIO.h"

// The utility library may include Infra (spec 048, the amended Principle VIII).
int readerChannels() {
    return kPixelChannels + kImageIOVersion;
}

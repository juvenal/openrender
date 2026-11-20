#include "imageio/orImageIO.h"

// Texturing may use the Image I/O utility library through its published header.
int decodeVersion() {
    return kImageIOVersion;
}

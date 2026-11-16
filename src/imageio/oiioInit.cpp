// oiioInit.cpp -- the one-time OpenImageIO setup and the version query (spec 048, research R7).
//
// OIIO starts a worker pool and scans a plug-in search path by default; neither belongs inside the renderer. The
// first entry point of the library sets, once (std::call_once): `threads` 1, so no OIIO worker pool starts; the
// OpenEXR pool size 0, so an EXR read stays in the calling thread; and an empty `plugin_searchpath`, so only the
// readers and writers embedded in the library exist.
#include "orImageIO.h"

#include <OpenImageIO/imageio.h>

#include <cstdio>
#include <mutex>

namespace orimageio {

    namespace {

        std::once_flag initFlag;
        char versionText[32];

        void initialize() {
            OIIO::attribute("threads", 1);
            OIIO::attribute("exr_threads", 0);
            OIIO::attribute("plugin_searchpath", "");
            std::snprintf(versionText, sizeof(versionText), "%d.%d.%d", OIIO_VERSION_MAJOR, OIIO_VERSION_MINOR,
                          OIIO_VERSION_PATCH);
        }

    } // namespace

    const char *oiioVersion() {
        std::call_once(initFlag, initialize);
        return versionText;
    }

} // namespace orimageio

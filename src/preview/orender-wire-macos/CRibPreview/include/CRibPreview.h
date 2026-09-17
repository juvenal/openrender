// Swift-visible C module for libribpreview.
//
// This used to be a hand-maintained duplicate of ribpreview_api.h that had drifted (it declared
// ribcam_write/ribcam_replace, which ribpreview_api.h itself did not). Consolidated during
// spec 016 implementation: ribpreview_api.h (src/preview/) is now the single source of truth
// for the whole C ABI.
//
// SPM rejects header search paths outside the package root, so the real ribpreview_api.h is
// staged into this same directory at build time (see CMakeLists.txt's copy_if_different step).
// The staged copy is gitignored — do not edit ribpreview_api.h here; edit
// src/preview/ribpreview_api.h and rebuild.
#pragma once

#include "ribpreview_api.h"

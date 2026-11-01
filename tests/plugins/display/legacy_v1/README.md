# Frozen legacy display driver (spec 037)

These files are **frozen copies** of the base tree (`486decd`, the code of `HEAD` outside `specs/`):

| Here | Copied from |
|---|---|
| `file.cpp`, `file.h`, `file_tiff.*`, `file_png.*`, `file_base.*` | `src/display/file/` |
| `dsply_v1_frozen.h` | `src/display/dsply.h` |

Only the `#include "dsply.h"` lines of `file.cpp` and `file_base.h` were changed, to
`#include "dsply_v1_frozen.h"`. `file.h` is not compiled into the module (nothing includes it); it is
kept so the copy of the directory is complete.

**Do not edit them.** The module built from them is the driver "built before this spec" of SC-013: an
unversioned driver whose entry points and header are the version 1 ones. When the live
`src/display/` files change (the display 2.0 migration), these must stay as they are.

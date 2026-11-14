#!/usr/bin/env python3
"""Stop the self-contained bundle step from applying DESTDIR twice.

install(CODE) already prefixes the destination with $ENV{DESTDIR}. CMake's
file(INSTALL) then prefixes DESTDIR again, so Homebrew libraries land in
$DESTDIR$DESTDIR/opt/openrender/lib and the install_name_tool pass that
follows never sees them. file(COPY) does not consult DESTDIR.

The edit is in the generated build/cmake_install.cmake, not in the source
CMakeLists.txt.
"""

import sys
from pathlib import Path

OLD = (
    'file(INSTALL DESTINATION "${_libpath}"\n'
    '                         TYPE SHARED_LIBRARY FOLLOW_SYMLINK_CHAIN FILES "${_dep}")'
)
NEW = 'file(COPY "${_dep}" DESTINATION "${_libpath}" FOLLOW_SYMLINK_CHAIN)'


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("usage: patch-bundle-destdir.py BUILD/cmake_install.cmake")
    path = Path(sys.argv[1])
    text = path.read_text()
    if NEW in text and OLD not in text:
        return
    count = text.count(OLD)
    if count != 1:
        sys.exit(f"{path}: expected one bundle file(INSTALL), found {count}")
    path.write_text(text.replace(OLD, NEW, 1))


if __name__ == "__main__":
    main()

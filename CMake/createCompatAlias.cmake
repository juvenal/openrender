# createCompatAlias.cmake -- create a library compatibility alias symlink.
#
# Usage (script mode):
#   cmake -DDIR=<dir> -DTARGET_NAME=<real file> -DALIAS_NAME=<alias> -P createCompatAlias.cmake
#
# Creates DIR/ALIAS_NAME -> TARGET_NAME (a relative symlink), overwriting any
# existing file of that name.
#
# Some aliases differ from their target only in letter case (libribVector ->
# libRIBVector). On a case-insensitive filesystem -- the macOS default, so a
# typical /usr/local or /opt/homebrew prefix -- those two names are the SAME
# directory entry: create_symlink would replace the real library with a
# symlink pointing at itself. There the alias is also unnecessary, since the
# old spelling already resolves to the real file. So probe the volume itself:
# if the all-uppercase spelling of the real file resolves, the volume folds
# case and the alias is skipped.
#
# Deliberately NOT "skip if ALIAS_NAME already exists": on a case-sensitive
# prefix that still holds a pre-rename real file (libribVector.dylib from an
# older install), that would leave the stale library posing as the alias.

foreach(_var DIR TARGET_NAME ALIAS_NAME)
    if(NOT DEFINED ${_var} OR "${${_var}}" STREQUAL "")
        message(FATAL_ERROR "createCompatAlias.cmake: ${_var} is required")
    endif()
endforeach()

if(NOT EXISTS "${DIR}/${TARGET_NAME}")
    message(FATAL_ERROR "createCompatAlias.cmake: ${DIR}/${TARGET_NAME} does not exist")
endif()

string(TOUPPER "${TARGET_NAME}" _upper)
if(NOT _upper STREQUAL TARGET_NAME AND EXISTS "${DIR}/${_upper}")
    # Case-insensitive volume. Only a pure case-variant alias is redundant
    # here; a genuinely different name (libri -> libRIBRender) still needs
    # its link.
    string(TOUPPER "${ALIAS_NAME}" _alias_upper)
    if(_alias_upper STREQUAL _upper)
        return()
    endif()
endif()

file(REMOVE "${DIR}/${ALIAS_NAME}")
file(CREATE_LINK "${TARGET_NAME}" "${DIR}/${ALIAS_NAME}" SYMBOLIC)

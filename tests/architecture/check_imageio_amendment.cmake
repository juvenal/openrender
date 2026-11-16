# ImageIO_AmendmentAgree (spec 048, T021, FR-006): the places that carry constitution 1.5.0's Image I/O amendment must
# agree. A cmake -P script with no git: it reads the tracked files by path.
#
#   cmake -DROOT=<source dir> -P check_imageio_amendment.cmake
#
# Checks: the constitution says version 1.5.0 and has the "Utility libraries" paragraph and the "Image I/O" table row;
# domain-rules.txt says v1.5.0 and holds the three rows; domain-map.txt maps src/imageio/ to ImageIO;
# DOMAIN_BOUNDARIES.md says v1.5.0 and names ImageIO; the plan template's Principle VIII gate names utility libraries;
# and archcheck knows the class ImageIO.
if(NOT ROOT)
    message(FATAL_ERROR "ImageIO_AmendmentAgree: pass -DROOT=<source dir>")
endif()

set(failures 0)

function(read_text var path)
    if(NOT EXISTS "${ROOT}/${path}")
        message(SEND_ERROR "FAIL: ${path} does not exist")
        set(${var} "" PARENT_SCOPE)
        return()
    endif()
    file(READ "${ROOT}/${path}" text)
    set(${var} "${text}" PARENT_SCOPE)
endfunction()

# A function (not a macro): a macro would re-parse the backslashes of the regexes (policy CMP0219).
function(expect text_var regex what)
    string(REGEX MATCH "${regex}" m "${${text_var}}")
    if(NOT m)
        message(STATUS "FAIL: ${what}")
        math(EXPR n "${failures}+1")
        set(failures ${n} PARENT_SCOPE)
    else()
        message(STATUS "ok:   ${what}")
    endif()
endfunction()

read_text(constitution ".specify/memory/constitution.md")
expect(constitution "\\*\\*Version\\*\\*: 1\\.5\\.0 " "constitution footer says Version 1.5.0")
expect(constitution "Version change: 1\\.4\\.0 → 1\\.5\\.0" "constitution Sync Impact Report says 1.4.0 -> 1.5.0")
expect(constitution "\\*\\*Utility libraries\\.\\*\\*" "constitution has the 'Utility libraries' paragraph")
expect(constitution "\\| *Image I/O" "constitution has the 'Image I/O' table row")

read_text(rules "tests/architecture/domain-rules.txt")
expect(rules "constitution v1\\.5\\.0" "domain-rules.txt header says v1.5.0")
expect(rules "(^|\n)Texturing +-> +ImageIO( |\n)" "domain-rules.txt: Texturing -> ImageIO")
expect(rules "(^|\n)Display +-> +ImageIO( |\n)" "domain-rules.txt: Display -> ImageIO")
expect(rules "(^|\n)ImageIO +->( *\n)" "domain-rules.txt: ImageIO -> (nothing)")

read_text(map "tests/architecture/domain-map.txt")
expect(map "(^|\n)prefix +src/imageio/ +ImageIO( |\n)" "domain-map.txt maps src/imageio/ to ImageIO")

read_text(devnotes "DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md")
expect(devnotes "constitution v1\\.5\\.0" "DOMAIN_BOUNDARIES.md says constitution v1.5.0")
expect(devnotes "ImageIO" "DOMAIN_BOUNDARIES.md names ImageIO")

read_text(plan ".specify/templates/plan-template.md")
expect(plan "[Uu]tility librar" "plan template's Principle VIII gate names utility libraries")

read_text(archconfig "tests/architecture/archcheck/config.cpp")
expect(archconfig "\"ImageIO\"" "archcheck's class list names ImageIO")

if(failures GREATER 0)
    message(FATAL_ERROR "ImageIO_AmendmentAgree: ${failures} place(s) do not carry the 1.5.0 amendment")
endif()
message(STATUS "ImageIO_AmendmentAgree: every place agrees")

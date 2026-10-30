# Display pixel baselines (spec 037, FR-021, SC-004, SC-013; plan R16).
#
# Deterministic 32 x 32 scenes, one per display-driver behaviour the migration touches, rendered
# through the in-tree drivers and through the frozen pre-spec file driver (display/legacy_v1/).
# Output is compared as decoded pixels (plugin_abi_pixel_compare, spec 018's CImageInput), with zero
# differing samples, against references captured from the unmodified tree:
#
#   DisplayAbi_PixelsIdentical        the in-tree drivers (file, rgbe, openexr)
#   DisplayAbi_LegacyPixelsIdentical  the frozen legacy file driver, against the SAME references
#
# Both are green on the starting tree by design: they are the evidence that the migration changes no
# pixel. The references are written only by the plugin-abi-capture-baselines build target (never by a
# ctest), which refuses to overwrite one unless the tree was configured with -DOR037_RECAPTURE=ON.
# Reference directories are per platform (reference/macos/, reference/linux/); a platform without its
# own directory compares against reference/macos/ and says so.

add_executable(plugin_abi_pixel_compare pixel_compare.cpp)
target_link_libraries(plugin_abi_pixel_compare PRIVATE openrender_common_flags ribRender openrendercommon)
target_include_directories(plugin_abi_pixel_compare PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/texture ${CMAKE_BINARY_DIR})
target_compile_features(plugin_abi_pixel_compare PRIVATE cxx_std_20)

set(BASELINE_RIB_DIR ${OR_PLUGIN_BIN}/display_baseline/rib)
set(BASELINE_LEGACY_RIB_DIR ${OR_PLUGIN_BIN}/display_baseline/rib_legacy)
set(BASELINE_SCENES "")
set(BASELINE_LEGACY_SCENES "")
set(BASELINE_GAMMA_SCENES "")

# or_baseline_scene(<name> <device> <file> <mode> [PRE <rib lines>] [PARAMS <display parameters>] [NO_LEGACY] [GAMMA])
function(or_baseline_scene name device file mode)
    cmake_parse_arguments(ARG "NO_LEGACY;GAMMA" "PRE;PARAMS" "" ${ARGN})
    set(SCENE_NAME ${name})
    set(SCENE_DEVICE ${device})
    set(SCENE_FILE ${file})
    set(SCENE_MODE ${mode})
    set(SCENE_PRE "${ARG_PRE}")
    set(SCENE_PARAMS "${ARG_PARAMS}")

    set(SCENE_DISPLAY_PATH "${OR_DISPLAY_PATH}")
    or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/base_scene.rib.in ${BASELINE_RIB_DIR}/${name}.rib)
    set(_scenes "${BASELINE_SCENES};${name}=${file}")
    set(BASELINE_SCENES "${_scenes}" PARENT_SCOPE)

    if(NOT ARG_NO_LEGACY)
        set(SCENE_DISPLAY_PATH "${OR_PLUGIN_BIN}/display/legacy_v1")
        or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/base_scene.rib.in ${BASELINE_LEGACY_RIB_DIR}/${name}.rib)
        set(_legacy "${BASELINE_LEGACY_SCENES};${name}=${file}")
        set(BASELINE_LEGACY_SCENES "${_legacy}" PARENT_SCOPE)
    endif()
    if(ARG_GAMMA)
        set(_gamma "${BASELINE_GAMMA_SCENES};${name}")
        set(BASELINE_GAMMA_SCENES "${_gamma}" PARENT_SCOPE)
    endif()
endfunction()

# TIFF: 8, 16 and 32-bit float (the bit depth follows the quantizer: 255, 65535, 0).
or_baseline_scene(tiff8 tiff tiff8.tif rgba)
or_baseline_scene(tiff16 file tiff16.tif rgba PRE "Quantize \"rgba\" 65535 0 65535 0.5")
or_baseline_scene(tifffloat file tifffloat.tif rgba PRE "Quantize \"rgba\" 0 0 0 0")
# PNG: 8 and 16-bit, chosen by the device ("png") or by the extension.
or_baseline_scene(png8 png png8.png rgba)
or_baseline_scene(png16 file png16.png rgba PRE "Quantize \"rgba\" 65535 0 65535 0.5")
# OpenEXR (the driver stores half-float whatever the quantizer) and Radiance RGBE.
if(HAVE_OPENEXR)
    or_baseline_scene(exrhalf openexr exrhalf.exr rgba NO_LEGACY)
endif()
or_baseline_scene(rgbe rgbe rgbe.hdr rgb NO_LEGACY)
# A depth (z) display and an AOV display with several channels.
or_baseline_scene(depth file depth.tif z)
or_baseline_scene(aov file aov.tif "rgba,N" PRE "DisplayChannel \"normal N\"")
# Dithering: an 8-bit output with a strong dither amplitude (the dither is a hash of pixel and channel).
or_baseline_scene(dither file dither.tif rgba PRE "Quantize \"rgba\" 255 0 255 1.0")
# Display-line overrides: a per-display quantize, and "type" choosing TIFF data under a .png name.
or_baseline_scene(quantline file quantline.tif rgba PARAMS "\"float[4] quantize\" [0 65535 0 65535]")
or_baseline_scene(typetiff file typetiff.png rgba PARAMS "\"string type\" [\"tiff\"]")
# A .png name sent to "file" with no type: PNG chosen by the extension.
or_baseline_scene(notype file notype.png rgba)
# Exposure gamma other than 1: gamma reaches the pixels and the PNG gAMA chunk (checked separately).
or_baseline_scene(gamma png gamma.png rgba PRE "Exposure 1 2.2" GAMMA)

set(_driver_targets file rgbe)
if(HAVE_OPENEXR)
    list(APPEND _driver_targets openexr)
endif()

or_plugin_scene_set(TEST DisplayAbi_PixelsIdentical
    RIB_DIR ${BASELINE_RIB_DIR}
    SCENES ${BASELINE_SCENES}
    GAMMA ${BASELINE_GAMMA_SCENES}
    DEPENDS ${_driver_targets})

or_plugin_scene_set(TEST DisplayAbi_LegacyPixelsIdentical
    RIB_DIR ${BASELINE_LEGACY_RIB_DIR}
    SCENES ${BASELINE_LEGACY_SCENES}
    GAMMA ${BASELINE_GAMMA_SCENES}
    VERIFY_ONLY
    DEPENDS plugin_display_legacy_file)

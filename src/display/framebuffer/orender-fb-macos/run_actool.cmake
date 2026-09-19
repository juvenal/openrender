# run_actool.cmake — invoked via cmake -P during the bundle assembly step.
# Compiles AppIcon.icon (Apple Icon Composer package) into Assets.car for
# macOS 26+ Liquid Glass icon support.  Silently skips if the .icon package
# is absent so that plain .icns-only builds continue to work.
#
# Required variables (passed via -D on the cmake -P command line):
#   ICON_PKG      — path to AppIcon.icon (the .icon package directory)
#   RESOURCES_DIR — path to Contents/Resources/ inside the .app bundle

if(NOT EXISTS "${ICON_PKG}")
    return()
endif()

find_program(XCRUN xcrun)
if(NOT XCRUN)
    message(WARNING "xcrun not found — AppIcon.icon will not be compiled (Liquid Glass icon unavailable)")
    return()
endif()

execute_process(
    COMMAND "${XCRUN}" actool
            "${ICON_PKG}"
            --app-icon AppIcon
            --compile "${RESOURCES_DIR}"
            --platform macosx
            --minimum-deployment-target 12.0
            --target-device mac
            --output-partial-info-plist /dev/null
    RESULT_VARIABLE actool_result
    ERROR_VARIABLE  actool_err
)

if(actool_result AND NOT actool_result EQUAL 0)
    message(WARNING "actool failed (exit ${actool_result}) — Liquid Glass icon unavailable\n${actool_err}")
endif()

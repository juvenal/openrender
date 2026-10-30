# The one host-side version check (spec 037, FR-003, FR-004, FR-007, FR-009): PluginAbi_VersionCheck.
#
# An in-process unit test of checkPluginVersion over a fake module (version_fake.c) that exports a
# version symbol of every shape data-model.md section 2 needs. It was written before pluginAbi.h and
# orplugin.h existed (EXCLUDE_FROM_ALL + DISABLED, red state: the compile error of
# `cmake --build build --target test_plugin_version_check`); T047 removed the three markers.
# (PluginAbi_SingleCheckRoutine lives in checks/.)

or_plugin_test_module(plugin_versioncheck_fake versioncheck version_fake version_fake.c)

add_executable(test_plugin_version_check test_version_check.cpp)
target_link_libraries(test_plugin_version_check PRIVATE openrender_common_flags openrendercommon)
target_include_directories(test_plugin_version_check PRIVATE ${CMAKE_SOURCE_DIR}/src ${OR_PLUGIN_STAGE_INCLUDE})
target_compile_features(test_plugin_version_check PRIVATE cxx_std_20)
add_dependencies(test_plugin_version_check plugin_versioncheck_fake)

or_plugin_add_test(PluginAbi_VersionCheck 60
    COMMAND test_plugin_version_check $<TARGET_FILE:plugin_versioncheck_fake>)

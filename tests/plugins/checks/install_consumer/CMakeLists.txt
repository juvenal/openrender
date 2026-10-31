# Spec 037, PluginAbi_InstallLayouts (T102a, SC-011, SC-019): a consumer of an installed openRender.
# It uses find_package(openRender) and nothing else to reach the headers: no include_directories(), no -I.
cmake_minimum_required(VERSION 3.19)
project(or_install_consumer C)

find_package(openRender CONFIG REQUIRED)

# An object library: it is compiled, never linked, so the imported targets need no library file.
add_library(consumer OBJECT consumer.c)
target_link_libraries(consumer PRIVATE openRender::RIBRender)
set_target_properties(consumer PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

# sloinfo — renamed from rsloinfo; reads .rslo metadata (TODO: add .slo / LLVM JIT)
add_executable(sloinfo sloinfo.cpp)
target_link_libraries(sloinfo PRIVATE libshader_runtime openrendercommon)
target_include_directories(sloinfo PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/libshader/runtime
)
install(TARGETS sloinfo RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Backward-compatibility alias (scripts that call rsloinfo keep working)
install(CODE
    "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink
        sloinfo
        \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/rsloinfo)")

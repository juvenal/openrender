# sloinfo — unified .rslo / .slo shader metadata inspector
# Installed as sloinfo; rsloinfo is a symlink to the same binary.

add_executable(sloinfo sloinfo.cpp)
target_include_directories(sloinfo PRIVATE ${CMAKE_SOURCE_DIR}/src)

# sloinfo reads shaders through the shader-metadata API only (spec 033, FR-017; Sloinfo_MetadataApiOnly): it links libRSLInfo and
# nothing else of the shader libraries, and carries no LLVM of its own. libRSLInfo holds the inspection parser for .rslo and, in its
# shared build with the JIT, the bitcode metadata probe for .slo (LLVM sits inside that library). Never link libRSLRuntime beside
# it: the two share the flex/bison "rslo" globals. openrendercommon propagates from libRSLInfo.
target_link_libraries(sloinfo PRIVATE libRSLInfo)

install(TARGETS sloinfo RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# rsloinfo is a symlink to sloinfo; argv[0] detection selects .rslo mode.
install(CODE
    "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink
        sloinfo
        \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/rsloinfo)")

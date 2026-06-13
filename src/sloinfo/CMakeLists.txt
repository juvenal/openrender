# sloinfo — unified .rslo / .slo shader metadata inspector
# Installed as sloinfo; rsloinfo is a symlink to the same binary.

add_executable(sloinfo sloinfo.cpp)
target_link_libraries(sloinfo PRIVATE libshader_runtime)
# openrendercommon is not listed directly; it propagates from libshader_runtime.
target_include_directories(sloinfo PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/libshader/runtime
)

# When LLVM is available, link the shading library for CLLVMJitEngine::extractMetadataFromFile.
# LLVM itself propagates transitively from libshader_shading (PUBLIC LLVM deps).
if(OPENRENDER_HAVE_LLVM)
    target_link_libraries(sloinfo PRIVATE libshader_shading)
    target_include_directories(sloinfo PRIVATE
        ${CMAKE_SOURCE_DIR}/src/libshader/shading
        ${LLVM_INCLUDE_DIRS}
    )
    target_compile_definitions(sloinfo PRIVATE ${LLVM_DEFINITIONS})
endif()

install(TARGETS sloinfo RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# rsloinfo is a symlink to sloinfo; argv[0] detection selects .rslo mode.
install(CODE
    "execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink
        sloinfo
        \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/rsloinfo)")

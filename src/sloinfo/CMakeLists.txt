# sloinfo — unified .rslo / .slo shader metadata inspector
# Installed as sloinfo; rsloinfo is a symlink to the same binary.

add_executable(sloinfo sloinfo.cpp)
target_link_libraries(sloinfo PRIVATE libshader_runtime)
# openrendercommon is not listed directly; it propagates from libshader_runtime.
target_include_directories(sloinfo PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/libshader/runtime
)

# When LLVM is available, link libshader_jitmeta for CLLVMJitEngine::extractMetadataFromFile.
# This is a small standalone static library (see libshader/shading/CMakeLists.txt) built
# from only the metadata-probe half of llvmJit.cpp -- pure LLVM bitcode inspection, with
# no dependency on ri/CRenderer symbols. sloinfo deliberately links this instead of the
# full libshader_shading .dylib: that library also contains code (e.g. the RSLO grammar's
# parseShader(), CFilterLookup's Ri*StepFilter bindings) that references ri-only symbols.
# Those symbols are meant to be resolved only when ri.dylib loads the library, but macOS
# dyld binds data/vtable/RTTI references eagerly at image load regardless of whether the
# owning code path ever runs -- so linking the whole library into a standalone tool like
# sloinfo aborts at startup even though extractMetadataFromFile() itself never touches ri.
if(OPENRENDER_HAVE_LLVM)
    target_link_libraries(sloinfo PRIVATE libshader_jitmeta)
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

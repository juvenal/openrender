add_executable(rsloinfo rsloinfo.cpp)
target_link_libraries(rsloinfo rslo openrendercommon)
# rslo.h moved to src/libshader/runtime/; expose old "rslo/" path via include dir parent
target_include_directories(rsloinfo PRIVATE
    ${CMAKE_SOURCE_DIR}/src   # src/rslo/rslo.h forwards to libshader/runtime/rslo.h
)
install(TARGETS rsloinfo RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

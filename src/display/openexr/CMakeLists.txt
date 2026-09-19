if(HAVE_OPENEXR)
    file(GLOB openexr_headers *.h)

    add_library(openexr MODULE openexr.cpp ${openexr_headers})
    set_target_properties(openexr PROPERTIES PREFIX "" SUFFIX ".dsply")

    # Target names differ between OpenEXR 2.x and 3.x; the root CMakeLists.txt
    # resolves which set exists and puts it in OPENRENDER_OPENEXR_LIBS.
    # openexr.cpp itself is identical for both.
    target_link_libraries(openexr PRIVATE
        openrenderfilebase
        ${OPENRENDER_OPENEXR_LIBS}
    )

    install(TARGETS openexr LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")
endif()
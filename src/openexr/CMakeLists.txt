if(HAVE_OPENEXR)
    file(GLOB openexr_headers *.h)

    add_library(openexr MODULE openexr.cpp ${openexr_headers})
    set_target_properties(openexr PROPERTIES PREFIX "" SUFFIX ".dsply")

    target_link_libraries(openexr PRIVATE
        openrenderfilebase
        OpenEXR::OpenEXR
        Imath::Imath
    )

    install(TARGETS openexr LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")
endif()
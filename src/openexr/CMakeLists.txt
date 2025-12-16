if(HAVE_OPENEXR)
    file(GLOB openexr_headers *.h)

    add_library(openexr MODULE openexr.cpp ${openexr_headers})
    set_target_properties(openexr PROPERTIES PREFIX "")

    # Use modern CMake imported targets
    target_link_libraries(openexr PRIVATE
        pixiecommon
        OpenEXR::OpenEXR
        Imath::Imath
    )

    install(TARGETS openexr LIBRARY DESTINATION "${PIXIE_DISPLAYSDIR}")
endif()
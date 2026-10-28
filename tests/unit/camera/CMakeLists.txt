# Camera layer tests (spec 028, camera-and-motion-layer). Every add_test() here carries the "camera"
# label (ctest -L '^camera$'; anchored, because -L is a regular expression).
#
# These are State-only tests (research R16): they compile against the State-only library of
# tests/unit/state (the State sources on a narrow include path, linked with libRIBCommon only), so they
# also prove the camera layer includes nothing outside Graphics State and Infra. They exist only when
# OPENRENDER_CAMERA_MOTION_ONLY_TESTS is ON, which also adds frameParams.cpp and camera.cpp to that
# library; the option is OFF until task T045, because both files are created in the foundation phase.

if(OPENRENDER_CAMERA_MOTION_ONLY_TESTS)
    # add_camera_motion_test(NAME EXE LABEL SOURCES... [ENABLED] [INCLUDES dir...])
    # Same convention as add_state_only_test: until the phase that provides the API enables it, the
    # executable is EXCLUDE_FROM_ALL and the test DISABLED, so the main build and every checkpoint stay
    # green; its red state is a build failure of the executable. Also used by tests/unit/motion.
    # INCLUDES: a test's own extra include directories (never src/ri/render, src/libshader,
    # src/ri/hiders or src/ri/core: the narrow path is the isolation proof).
    function(add_camera_motion_test NAME EXE LABEL)
        cmake_parse_arguments(ARG "ENABLED" "" "INCLUDES" ${ARGN})
        if(ARG_ENABLED)
            add_executable(${EXE} ${ARG_UNPARSED_ARGUMENTS})
        else()
            add_executable(${EXE} EXCLUDE_FROM_ALL ${ARG_UNPARSED_ARGUMENTS})
        endif()
        target_link_libraries(${EXE} PRIVATE state_only_sources)
        target_compile_features(${EXE} PRIVATE cxx_std_20)
        # frame_test_utils.h is shared by the camera and motion tests
        target_include_directories(${EXE} PRIVATE ${CMAKE_SOURCE_DIR}/tests/unit/camera ${ARG_INCLUDES})
        add_test(NAME ${NAME} COMMAND ${EXE})
        set_tests_properties(${NAME} PROPERTIES LABELS "${LABEL};unit" TIMEOUT 60)
        if(NOT ARG_ENABLED)
            set_tests_properties(${NAME} PROPERTIES DISABLED TRUE)
        endif()
    endfunction()

    add_camera_motion_test(Camera_Analytic test_camera_analytic camera test_camera_analytic.cpp)
    add_camera_motion_test(Camera_StartingTreeEquivalence test_camera_equivalence camera test_camera_equivalence.cpp)
    add_camera_motion_test(Camera_CropInvariance test_camera_crop camera test_camera_crop.cpp)
    add_camera_motion_test(Camera_GenerateRayOnly test_camera_generate_ray_only camera test_camera_generate_ray_only.cpp)
    add_camera_motion_test(Camera_LensModel test_camera_lens_model camera test_camera_lens_model.cpp)
    add_camera_motion_test(Frame_DefaultValue test_frame_default camera test_frame_default.cpp)
endif()

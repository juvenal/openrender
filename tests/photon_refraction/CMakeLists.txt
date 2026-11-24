# GitHub #211: the photon hider's refracting shading models (glass, water, dielectric) obey Snell's law and focus a ball lens.
# test_photon_refraction renders a photon pass with orender, reads the caustic map it wrote and checks a physical property
# (the stored direction against Snell's law, the focus against the paraxial image plane); no recorded image or hash is involved.
add_executable(test_photon_refraction test_photon_refraction.cpp)
target_compile_features(test_photon_refraction PRIVATE cxx_std_20)

set(PR_DISPLAY_PATH "$<TARGET_FILE_DIR:file>")
set(PR_ENV
    "SHADERS=${CMAKE_BINARY_DIR}/shaders"
    "ORENDERHOME=${CMAKE_BINARY_DIR}/tests/visual/_home"
    "DISPLAYS=${PR_DISPLAY_PATH}"
    "GEOMETRIES=${CMAKE_SOURCE_DIR}/geometry")

foreach(_mode entry exit focus)
    foreach(_model glass water dielectric)
        set(_name PhotonRefraction_${_mode}_${_model})
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/work/${_name})
        add_test(NAME ${_name}
            COMMAND test_photon_refraction ${_mode} ${_model} $<TARGET_FILE:orender> ${CMAKE_CURRENT_SOURCE_DIR}/scenes ${CMAKE_CURRENT_BINARY_DIR}/work/${_name})
        set_tests_properties(${_name} PROPERTIES TIMEOUT 300 LABELS "photon_refraction;photon" ENVIRONMENT "${PR_ENV}" DEPENDS orender)
    endforeach()
endforeach()

# the red end of the spectrum sees the first value of iorrange, the blue end the second (the model argument is not used)
set(_name PhotonRefraction_dispersion)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/work/${_name})
add_test(NAME ${_name}
    COMMAND test_photon_refraction dispersion dielectric $<TARGET_FILE:orender> ${CMAKE_CURRENT_SOURCE_DIR}/scenes ${CMAKE_CURRENT_BINARY_DIR}/work/${_name})
set_tests_properties(${_name} PROPERTIES TIMEOUT 300 LABELS "photon_refraction;photon" ENVIRONMENT "${PR_ENV}" DEPENDS orender)

# PROPOSED (tasks.md T123/T124, not applied): the registration of AreaLight_QuadlightAgreement{,-slo}, to be appended to
# tests/visual/arealight-scenes.cmake at the agreement PAUSE once the threshold is approved. The old and the new render of
# arealight-quadlight-agree-{old,new}-raytrace[-slo].rib compared by test_hider_parity's 8 x 8 block metric (diff of two renders).
set(_ql_threshold 8)
foreach(_fmt "" "-slo")
    set(_ql_scratch ${_arealight_dir}/AreaLight_QuadlightAgreement${_fmt})
    file(MAKE_DIRECTORY ${_ql_scratch})
    add_test(NAME AreaLight_QuadlightAgreement${_fmt}
        COMMAND test_hider_parity $<TARGET_FILE:orender>
            ${_arealight_rib}/arealight-quadlight-agree-old-raytrace${_fmt}.rib arealight-quadlight-agree-old-raytrace${_fmt}.tif
            ${_arealight_rib}/arealight-quadlight-agree-new-raytrace${_fmt}.rib arealight-quadlight-agree-new-raytrace${_fmt}.tif
            ${_ql_threshold}
        WORKING_DIRECTORY ${_ql_scratch})
    set_tests_properties(AreaLight_QuadlightAgreement${_fmt} PROPERTIES TIMEOUT 360 LABELS "arealight;visual" ENVIRONMENT "${_arealight_env}" DEPENDS orender)
    # an -slo scene without the JIT would silently run the interpreter, a guaranteed pass: disabled then
    if("${_fmt}" STREQUAL "-slo" AND NOT OPENRENDER_HAVE_LLVM)
        set_tests_properties(AreaLight_QuadlightAgreement${_fmt} PROPERTIES DISABLED TRUE)
    endif()
endforeach()

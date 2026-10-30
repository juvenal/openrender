# DLO fixtures and the FR-032 check (spec 037).
#
#   good            placed where an absolute path reaches it (the scenes load it by that path)
#   bare_name       placed only in a directory the procedural search path names
#   missing_symbol  no dloTini (ProceduresAbi_MissingSymbol characterizes the host's error text)

or_plugin_test_module(plugin_dlo_good dlo good dlo_fixture.cpp)
or_plugin_test_module(plugin_dlo_bare_name dlo bare_name dlo_fixture.cpp)
or_plugin_test_module(plugin_dlo_missing_symbol dlo missing_symbol dlo_fixture.cpp DEFS TEST_OMIT_TINI)

set(DLO_BARE_DIR ${OR_PLUGIN_BIN}/dlo/bare_name)
or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/bare_name.rib.in ${OR_PLUGIN_BIN}/dlo/bare_name.rib)

# FR-032 -- run from a directory that does not contain the module.
or_plugin_add_test(ProceduresAbi_BareNameLocated 120
    COMMAND ${CMAKE_COMMAND}
        -DORENDER=$<TARGET_FILE:orender>
        -DRIB=${OR_PLUGIN_BIN}/dlo/bare_name.rib
        -DWORKDIR=${OR_PLUGIN_BIN}/dlo/work/bare_name
        -DLOG=${OR_PLUGIN_BIN}/dlo/work/bare_name.log
        -DCOMMON=${OR_PLUGIN_COMMON_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_bare_name.cmake)
set_tests_properties(ProceduresAbi_BareNameLocated PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

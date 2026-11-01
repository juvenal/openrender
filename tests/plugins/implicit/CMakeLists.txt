# Implicit-surface fixtures (spec 037). The scenes and the characterization tests that use them
# live in tests/plugins/scenes/.
#
#   good            the full interface, 1.0.0 declared
#   missing_symbol  no implicitTini (ProceduresAbi_MissingSymbol)
#   legacy, v2, newer, hidden   as the dlo variants (tests/plugins/dlo/CMakeLists.txt)

or_plugin_test_module(plugin_implicit_good implicit good implicit_fixture.cpp)
or_plugin_test_module(plugin_implicit_missing_symbol implicit missing_symbol implicit_fixture.cpp DEFS TEST_OMIT_TINI)
or_plugin_test_module(plugin_implicit_legacy implicit legacy implicit_fixture.cpp DEFS TEST_NO_VERSION)
or_plugin_test_module(plugin_implicit_v2 implicit v2 implicit_fixture.cpp DEFS TEST_ABI_MAJOR=2 TEST_ABI_MINOR=0)
or_plugin_test_module(plugin_implicit_newer implicit newer implicit_fixture.cpp DEFS TEST_ABI_MAJOR=1 TEST_ABI_MINOR=9)
or_plugin_test_module(plugin_implicit_hidden implicit hidden implicit_fixture.cpp HIDDEN)

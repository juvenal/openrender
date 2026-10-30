# Implicit-surface fixtures (spec 037). The scenes and the characterization tests that use them
# live in tests/plugins/scenes/.
#
#   good            the full interface
#   missing_symbol  no implicitTini (ProceduresAbi_MissingSymbol)

or_plugin_test_module(plugin_implicit_good implicit good implicit_fixture.cpp)
or_plugin_test_module(plugin_implicit_missing_symbol implicit missing_symbol implicit_fixture.cpp DEFS TEST_OMIT_TINI)

# Test suite for JIT/interpreter shading-parity fixes (spec 014-jit-shading-parity)
#
# Executables in this directory link ri + libshader_shading + openrendercommon
# (the "tier-2" full-stack pattern, mirroring tests/imager/CMakeLists.txt) so
# they can load real .rslo/.slo shaders through CRenderer and compare backend
# behavior end-to-end. Every add_test() here MUST carry the "shading_parity"
# ctest label so `ctest -L shading_parity` selects the whole suite.
#
# add_executable() blocks are added per-user-story below as their oracle
# tests are written (T010, T019, T024) — intentionally empty at Setup time.

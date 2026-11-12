# Shared raster-path unit tests (spec 039, T144).
#
# Raster_QuadDepthRange: rasterQuadDepth() (src/ri/hiders/rasterEdge.h), the one depth
# interpolation the reyes and zbuffer quad paths share, never returns a depth outside the
# quad's vertex-depth range, and returns exactly the corners' depth on a constant-depth quad.
# Without the clamp, the bilinear form rounds one ulp below the corners on x86-64 (GCC 13), and
# the reyes opacity cull then drops a grid that would win an exact tie (GitHub #62 family,
# validation.md T143). Header-only: no library is linked.
add_executable(test_raster_depth test_raster_depth.cpp)
target_compile_features(test_raster_depth PRIVATE cxx_std_20)
target_include_directories(test_raster_depth PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/hiders)
add_test(NAME Raster_QuadDepthRange COMMAND test_raster_depth)
set_tests_properties(Raster_QuadDepthRange PROPERTIES TIMEOUT 60 LABELS "unit;determinism")

# Raster_OcclusionCullerMaxDepth: COcclusionCuller::touchNode() keeps every node at the maximum of
# its children, so the maximum opaque depth the reyes and zbuffer hiders cull against is never
# nearer than a live sample (GitHub #63). A first case where a group's last leaf to move nearer is
# not its farthest, then random nearer-only updates checked against the farthest leaf after each.
add_executable(test_occlusion_culler test_occlusion_culler.cpp)
target_compile_features(test_occlusion_culler PRIVATE cxx_std_20)
target_include_directories(test_occlusion_culler PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/hiders)
target_link_libraries(test_occlusion_culler PRIVATE ribRender openrendercommon)
add_test(NAME Raster_OcclusionCullerMaxDepth COMMAND test_occlusion_culler)
set_tests_properties(Raster_OcclusionCullerMaxDepth PROPERTIES TIMEOUT 60 LABELS "unit;determinism")

# Raster_QuadEdgeDegenerate: the quad coverage test of the reyes and zbuffer hiders accepts a sample
# inside a pole-like micropolygon whose two pole vertices are a few ulps apart (the sphere's pole row
# is not one point after projection), whichever way the gap points and the quad winds, and still
# rejects samples outside it, a needle's extension included (GitHub #57). Header-only.
add_executable(test_raster_edge test_raster_edge.cpp)
target_compile_features(test_raster_edge PRIVATE cxx_std_20)
target_include_directories(test_raster_edge PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/hiders)
add_test(NAME Raster_QuadEdgeDegenerate COMMAND test_raster_edge)
set_tests_properties(Raster_QuadEdgeDegenerate PROPERTIES TIMEOUT 60 LABELS "unit;determinism")

# Hiders_ReyesBucketBoundCoversCoC (spec 031, lens phase, FR-031): the depth-of-field expansion of the bucket bound and of a
# grid's bound (reyesLensBounds.h, the seam reyes.cpp and this test both call) covers every vertex scattered by the lens
# points on the unit circle, for boxes on both sides of the focal plane, spanning it, reaching the eye plane (the clamp)
# and reaching infinity. Renderer build (links ribRender like Raster_OcclusionCullerMaxDepth); state/camera.h is inline.
add_executable(test_reyes_lens_bounds test_reyes_lens_bounds.cpp)
target_compile_features(test_reyes_lens_bounds PRIVATE cxx_std_20)
target_include_directories(test_reyes_lens_bounds PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/state)
target_link_libraries(test_reyes_lens_bounds PRIVATE ribRender openrendercommon)
add_test(NAME Hiders_ReyesBucketBoundCoversCoC COMMAND test_reyes_lens_bounds)
set_tests_properties(Hiders_ReyesBucketBoundCoversCoC PROPERTIES TIMEOUT 60 LABELS "hiders;unit")

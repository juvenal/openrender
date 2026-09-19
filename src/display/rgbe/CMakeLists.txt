file(GLOB rgbe_headers *.h)
file(GLOB rgbe_sources *.cpp)

add_library(rgbe MODULE ${rgbe_sources} ${rgbe_headers})
set_target_properties(rgbe PROPERTIES PREFIX "" SUFFIX ".dsply")
target_link_libraries(rgbe PRIVATE openrenderfilebase)
install(TARGETS rgbe LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")

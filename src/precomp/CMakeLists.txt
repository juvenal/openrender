set(precomp_sources precomp.cpp)

add_executable(precomp ${precomp_sources})
target_link_libraries(precomp openrendercommon)

# no install, right?
#install(TARGETS precomp RUNTIME DESTINATION bin)

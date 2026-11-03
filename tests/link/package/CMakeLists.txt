# T-L4 Link_PackageStaticConsumer (spec 029): a consumer of an INSTALLED openRender, through the package and nothing else.
# It names no link option of its own: the Static component must be enough (FR-004, SC-002).
cmake_minimum_required(VERSION 3.19)
project(or029_package_consumer CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(openRender CONFIG REQUIRED COMPONENTS Static)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE openRender::RIBRender_static)

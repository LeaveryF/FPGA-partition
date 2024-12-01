project(BUPT_PART)
cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(include)

file(GLOB SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

link_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(partitioner ${SOURCES})
target_link_libraries(partitioner mtkahypar)

# add_executable(test_mt ${CMAKE_SOURCE_DIR}/examples/map_hypergraph_onto_target_graph.cc)
# target_link_libraries(test_mt mtkahypar)

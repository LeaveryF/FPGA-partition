project(BUPT_PART)
cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

file(GLOB SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

link_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(partitioner ${SOURCES})

target_link_libraries(partitioner mtkahypar)

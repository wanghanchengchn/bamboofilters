cmake_minimum_required(VERSION 3.5)

project(bamboofilters)

set(CMAKE_CXX_FLAGS
    "-fno-strict-aliasing -O3 -std=c++11 -march=core-avx2 ${CMAKE_CXX_FLAGS} -fopenmp"
)
set(CMAKE_EXE_LINKER_FLAGS "-lpthread ${CMAKE_EXE_LINKER_FLAGS}")
set(CMAKE_CXX_FLAGS "-fpermissive ${CMAKE_CXX_FLAGS}")

add_subdirectory(src)
add_subdirectory(test)

cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_COMPILER "clang++")
#This is just needed for cafe cause its still stuck on gcc 4.8. 
include_directories(/home/ts20/share/llvm9/llvm-project/build/projects/openmp/runtime/src/)

add_library (produce_consume-shared SHARED produce_consume.cpp)
add_library (produce_consume-static STATIC produce_consume.cpp)
target_compile_options(produce_consume-shared PUBLIC -std=c++11)
target_compile_options(produce_consume-static PUBLIC -std=c++11)



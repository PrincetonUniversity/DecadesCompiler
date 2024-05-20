cmake_minimum_required(VERSION 3.2)

Project(DecadesCompiler)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(DEC_INCLUDE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/include)

set(LLVM_REQ_VERSION "11.1" CACHE STRING "Requested LLVM Version.") 
find_package(LLVM ${LLVM_REQ_VERSION} REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

set(CMAKE_CXX_COMPILER ${LLVM_TOOLS_BINARY_DIR}/clang++)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)

add_subdirectory(passes)
add_subdirectory(RT)
add_subdirectory(utils)

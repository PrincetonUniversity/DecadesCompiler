cmake_minimum_required(VERSION 3.2)

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

add_subdirectory(AlwaysInliner)
add_subdirectory(DecoupleCompute)
add_subdirectory(DecoupleSupply)
add_subdirectory(ClangVisitor)
add_subdirectory(NumbaFunctionReplace)
add_subdirectory(PrintFunctionCalls)

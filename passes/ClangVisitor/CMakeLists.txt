cmake_minimum_required(VERSION 3.2)

find_package(LLVM REQUIRED CONFIG)

SET(CMAKE_CXX_COMPILER clang++)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "include dir: ${LLVM_INCLUDE_DIRS}")


set(CLANG_LIB_PATH "${LLVM_INCLUDE_DIRS}/../lib")


# Hacky because we are assuming that one directory down from the build dir,
# we can find a clang/include directory.
include_directories(APPEND ${LLVM_INCLUDE_DIRS} ${LLVM_BINARY_DIR}/../clang/include;${LLVM_BINARY_DIR}/tools/clang/include)
link_directories(${CLANG_LIB_PATH})

add_executable (DECVisitor DECVisitor.cpp)

target_compile_features(DECVisitor PRIVATE cxx_range_for cxx_auto_type)

target_compile_options(DECVisitor PUBLIC -fno-rtti)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs all)

# Link against LLVM libraries
target_link_libraries(DECVisitor ${llvm_libs})

target_link_libraries(DECVisitor
	-Wl,--start-group
	 clangAST 
	 clangASTMatchers 
	 clangAnalysis 
	 clangBasic 
	 clangDriver 
	 clangEdit 
	 clangFrontend 
	 clangFrontendTool 
	 clangLex 
	 clangParse 
	 clangSema 
	 clangEdit 
	 clangRewrite 
	 clangRewriteFrontend 
	 clangStaticAnalyzerFrontend 
	 clangStaticAnalyzerCheckers 
	 clangStaticAnalyzerCore 
	 clangCrossTU 
	 clangIndex 
	 clangSerialization 
	 clangToolingCore 
	 clangTooling 
	 clangFormat
	 -Wl,--end-group)

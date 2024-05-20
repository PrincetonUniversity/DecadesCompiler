add_custom_command(COMMENT "producing llvm for kernel profiler"
	           OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/KernelProfiler.ll
		   COMMAND ${LLVM_TOOLS_BINARY_DIR}/clang++ -std=c++11 -S -emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/KernelProfiler.cpp -o ${CMAKE_CURRENT_BINARY_DIR}/KernelProfiler.ll
		   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/KernelProfiler.cpp
		   BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/KernelProfiler.ll)

add_custom_target(run3 ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/KernelProfiler.ll)

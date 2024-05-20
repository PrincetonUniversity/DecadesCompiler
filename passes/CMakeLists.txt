add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

add_library(PrintFunctionCallsPass MODULE PrintFunctionCalls.cpp)

set_target_properties(PrintFunctionCallsPass PROPERTIES COMPILE_FLAGS "-fno-rtti")

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
  set_target_properties(PrintFunctionCallsPass PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif(APPLE)

### IMPORTED FROM ./ClangVisitor
add_executable (DECVisitor DECVisitor.cpp)
target_compile_options(DECVisitor PUBLIC -fno-rtti)

execute_process(COMMAND ${LLVM_TOOLS_BINARY_DIR}/llvm-config --libs all
  OUTPUT_VARIABLE llvm_libs)
 
string(STRIP ${llvm_libs} llvm_libs)
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

### IMPORTED FROM ./FindNMA
add_library(FindNMAPass MODULE FindNMA.cpp)

set_target_properties(FindNMAPass PROPERTIES COMPILE_FLAGS "-fno-rtti")
if(APPLE)
    set_target_properties(FindNMAPass PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif(APPLE)
### IMPORTED FROM ./DecoupleSupply
add_library(DecoupleSupplyPass MODULE DecoupleSupply.cpp)

set_target_properties(DecoupleSupplyPass PROPERTIES COMPILE_FLAGS "-fno-rtti")

add_library(DecoupleSupplyAddrPass MODULE DecoupleSupplyAddr.cpp)

set_target_properties(DecoupleSupplyAddrPass PROPERTIES COMPILE_FLAGS "-fno-rtti")

if(APPLE)
    set_target_properties(DecoupleSupplyPass PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif(APPLE)

add_library(AlwaysInlinerPass MODULE AlwaysInliner.cpp)

set_target_properties(AlwaysInlinerPass PROPERTIES COMPILE_FLAGS "-fno-rtti")

if(APPLE)
    set_target_properties(AlwaysInlinerPass PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)

add_library(DecoupleComputePass MODULE DecoupleCompute.cpp)

set_target_properties(DecoupleComputePass PROPERTIES COMPILE_FLAGS "-fno-rtti")

if(APPLE)
    set_target_properties(DecoupleComputePass PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)

#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/InstIterator.h"

using namespace std;
using namespace llvm;

namespace {

#include "Decouple.h"
  std::map<std::string, int> seen_functions;
  
  void print_kernel_call_funcs(Module &M, Function &f) {
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa<CallInst>(*iI)) {	
	CallInst *tmp = dyn_cast<CallInst>(&(*iI));
	if (tmp->getCalledFunction() != NULL) {
	  auto to_store = tmp->getCalledFunction()->getName().str();
	  if (seen_functions.find(to_store) == seen_functions.end()) {
	    seen_functions[to_store] = 0;
	  }
	  seen_functions[to_store]++;
	} else {
	  errs() << "[warning: indirect function call found\n";
	}
      }
    }
  }
  
  bool isKernelFunction(Function &func) {
    if (
	((func.getName().str().find(SUPPLY_KERNEL_STR)  != std::string::npos) ||
	 (func.getName().str().find(COMPUTE_KERNEL_STR) != std::string::npos)) &&
	(func.getName().str().find("cpython") == std::string::npos)
	) {
      //errs() << "found kernel:\n";
      //errs() << func.getName().str() << "\n";
      return true;
    }
    return false;  
  }
  


  struct PrintFunctionCallsPass : public ModulePass {
    static char ID;
    PrintFunctionCallsPass() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
      errs() << "[Print Function Calls Pass Begin]\n";

      for (Module::iterator fI = M.begin(), fE = M.end(); fI != fE; ++fI) {
	if (isKernelFunction(*fI)) {
	  errs() << "[Found Kernel]\n";	  
	  errs() << "[" << fI->getName().str() << "]\n";
	  print_kernel_call_funcs(M, *fI);
	}
      }

      errs() << "[functions not inlined]\n";

      for ( const auto &myPair : seen_functions ) {
	errs() << myPair.first << " : " << myPair.second << "\n";
      }

      errs() << "[Print Function Calls Pass Finished]\n";
      return true;

    }
  };
}

char PrintFunctionCallsPass::ID = 0;
static RegisterPass<PrintFunctionCallsPass> X("printfunctioncalls", "Print Function Call Pass", false, false);

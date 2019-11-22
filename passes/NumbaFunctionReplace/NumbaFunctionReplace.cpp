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

  Function *decades_barrier;
  vector<Instruction *> to_erase;
  map<string, string> intrinsic_map = {
    {"PyDECADES_fetchAdd", "DECADES_FetchAdd"},
    {"PyDECADES_atomicAdd", "DECADES_AtomicAdd"}
  };

  void populate_functions(Module &M) {

    decades_barrier = (Function *) M.getOrInsertFunction("DECADES_BARRIER",
                                                   FunctionType::get(Type::getVoidTy(M.getContext()), false)).getCallee();
  }

  bool isKernelFunction(Function &func) {
    return (func.getName().str().find("_kernel_") != std::string::npos)
      &&
      (func.getName().str().find("cpython") == std::string::npos);
  }

  string intrinsic_available(CallInst *I) {
    if (I->getCalledFunction() == NULL) {
      return string();
    }

    string calledName = I->getCalledFunction()->getName().str();
    for (auto f = intrinsic_map.begin(); f != intrinsic_map.end(); ++f) {
      if (calledName.find(f->first) != std::string::npos) {
        return f->second;
      }
    }

    return string();
  }

  void replace_intrinsic(Module &M, CallInst *I, string intrinsic) {
    auto FT = I->getFunctionType();
    Function * to_insert = (Function *) M.getOrInsertFunction(intrinsic, FT).getCallee();
    IRBuilder<> Builder(I);
    std::vector<Value *> Args(I->arg_begin(), I->arg_end());
    Instruction *Intr = Builder.CreateCall(to_insert, Args);

    I->replaceAllUsesWith(Intr);
    to_erase.push_back(I);
    errs() << "Replacing call for " << intrinsic << "\n";
  }

  bool barrier_needs_to_be_replaced(CallInst *I) {
    // can make this more general

    if (I->getCalledFunction() == NULL) {
      return false;
    }

    if (I->getCalledFunction()->getName().str().find("PyDECADES_barrier") != std::string::npos ) {
      return true;
    }

    return false;
  }

  void replace_barrier(Module &M, CallInst *I) {
    if (I->getCalledFunction()->getName().str().find("PyDECADES_barrier") != std::string::npos ) {
      IRBuilder<> Builder(I);
      Instruction *Intr = Builder.CreateCall(decades_barrier);
      auto V = ConstantInt::get(Type::getInt32Ty(M.getContext()),0);

      I->replaceAllUsesWith(V);
      to_erase.push_back(I);
      errs() << "Replacing one barrier call\n";
    }
  }

  void instrument_kernel(Module &M, Function &f) {
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa<CallInst>(*iI)) {
        CallInst *tmp = dyn_cast<CallInst>(&(*iI));
        if (barrier_needs_to_be_replaced(tmp)) {
          replace_barrier(M, tmp);
        }

        string intrinsic = intrinsic_available(tmp);
        if (!intrinsic.empty()) {
          replace_intrinsic(M, tmp, intrinsic);
        }
      }
    }
  }


  struct NumbaFunctionReplacePass : public ModulePass {
    static char ID;
    NumbaFunctionReplacePass() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
      errs() << "[Numba Function Replace Pass Begin]\n";

      populate_functions(M);

      for (Module::iterator fI = M.begin(), fE = M.end(); fI != fE; ++fI) {
        if (isKernelFunction(*fI)) {
          errs() << "[Found Kernel]\n";
          errs() << "[" << fI->getName().str() << "]\n";
          instrument_kernel(M, *fI);
        }
      }

      errs() << "Erasing functions\n";
      for (int i = 0; i < to_erase.size(); i++) {
        to_erase[i]->eraseFromParent();
      }

      errs() << "[Numba Function Replace Pass Finished]\n";
      return true;
    }
  };
}

char NumbaFunctionReplacePass::ID = 0;
static RegisterPass<NumbaFunctionReplacePass> X("numbafunctionreplace", "Numba Function Replace Pass", false, false);

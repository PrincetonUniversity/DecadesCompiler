//===- InlineAlways.cpp - Code to inline always_inline functions ----------===//
 //
 //                     The LLVM Compiler Infrastructure
 //
 // This file is distributed under the University of Illinois Open Source
 // License. See LICENSE.TXT for details.
 //
 //===----------------------------------------------------------------------===//
 //
 // This file implements a custom inliner that handles only functions that
 // are marked as "always inline".
 //
 //===----------------------------------------------------------------------===//
 
#include "llvm/Transforms/IPO/AlwaysInliner.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/InitializePasses.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/Inliner.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include <set>
#include <map>
#include "assert.h"
using namespace llvm;

 
#define DEBUG_TYPE "inline"
#define SUPPLY_KERNEL_STR "_kernel_supply"
#define COMPUTE_KERNEL_STR "_kernel_compute"


std::set<Instruction*> notInlined;
std::set<Function*>* inlineSet = new std::set<Function*>();
std::vector<std::string> accelerators{ "decadesTF_matmul", "decadesTF_sdp", "decadesTF_conv2d_layer", "decadesTF_dense_layer", "decadesTF_bias_add" };

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

void setInlineSet(Function* F, std::set<Function*>* inlineSet) { 
  for (auto& B:*F) {
    for (auto &I : B) {      
      if (isa<CallInst>(I)) {
        Instruction* Inst = &I;
        Function *f = ((CallInst*)Inst)->getCalledFunction();
        //errs() << "CALLED FUNCTION: " << f->getName() << "\n";
        if (!f->isDeclaration()  /*&&  isInlineViable(*f)*/) {
          inlineSet->insert(f);          
        }
        else {
          //errs() << "Couldn't INLINE: \n";
          notInlined.insert(&I);
        }
        setInlineSet(f, inlineSet);
        //I.dump();
        //errs() << I.getName().str() << "\n";
      }
    }
  }
}

PreservedAnalyses AlwaysInlinerPass::run(Module &M, ModuleAnalysisManager &) {

  InlineFunctionInfo IFI;
  
  SmallSetVector<CallBase *, 16> Calls;
  bool Changed = false;
  SmallVector<Function *, 16> InlinedFunctions; 
  for (Function &F : M) {
    if(isKernelFunction (F)) {
      setInlineSet(&F, inlineSet);
    }     
  }
  
  errs() << "Couldn't Inline: \n";
  for (auto& I: notInlined) {
    errs() << I << "\n";    
  }
  
  for (Function &F : M) {
    if (!F.isDeclaration() && inlineSet->find(&F)!=inlineSet->end()) {
      Calls.clear();
      
      for (User *U : F.users())
        if (auto *CB = dyn_cast<CallBase>(U))
          if (CB->getCalledFunction() == &F)
            Calls.insert(CB);
      
      for (CallBase *CB : Calls)
        // FIXME: We really shouldn't be able to fail to inline at this point!
        // We should do something to log or check the inline failures here.
        Changed |=
          InlineFunction(*CB, IFI, /*CalleeAAR=*/nullptr, InsertLifetime).isSuccess();
      
      // Remember to try and delete this function afterward. This both avoids
      // re-walking the rest of the module and avoids dealing with any iterator
      // invalidation issues while deleting functions.
      InlinedFunctions.push_back(&F);
    }
    
    // Remove any live functions.
    erase_if(InlinedFunctions, [&](Function *F) {
                                 F->removeDeadConstantUsers();
                                 return !F->isDefTriviallyDead();
                               });
    
    // Delete the non-comdat ones from the module and also from our vector.
    auto NonComdatBegin = partition(
                                    InlinedFunctions, [&](Function *F) { return F->hasComdat(); });
    for (Function *F : make_range(NonComdatBegin, InlinedFunctions.end()))
      M.getFunctionList().erase(F);
    InlinedFunctions.erase(NonComdatBegin, InlinedFunctions.end());
    
    if (!InlinedFunctions.empty()) {
      // Now we just have the comdat functions. Filter out the ones whose comdats
      // are not actually dead.
      filterDeadComdatFunctions(M, InlinedFunctions);
      // The remaining functions are actually dead.
      for (Function *F : InlinedFunctions)
        M.getFunctionList().erase(F);
    }
  }
  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

namespace {
 
 /// Inliner pass which only handles "always inline" functions.
 ///
 /// Unlike the \c AlwaysInlinerPass, this uses the more heavyweight \c Inliner
 /// base class to provide several facilities such as array alloca merging.
  class AlwaysInlinerLegacyPass : public LegacyInlinerBase {
 
 public:
   AlwaysInlinerLegacyPass() : LegacyInlinerBase(ID, /*InsertLifetime*/ true) {
     initializeAlwaysInlinerLegacyPassPass(*PassRegistry::getPassRegistry());
   }

 
   AlwaysInlinerLegacyPass(bool InsertLifetime)
       : LegacyInlinerBase(ID, InsertLifetime) {
     initializeAlwaysInlinerLegacyPassPass(*PassRegistry::getPassRegistry());
   }
 
   /// Main run interface method.  We override here to avoid calling skipSCC().
   bool runOnSCC(CallGraphSCC &SCC) override {
     return inlineCalls(SCC); }
 
   static char ID; // Pass identification, replacement for typeid
 
   InlineCost getInlineCost(CallBase &CS) override;
   virtual StringRef getPassName() const override;
   using llvm::Pass::doFinalization;
   bool doFinalization(CallGraph &CG) override {
     return removeDeadFunctions(CG, /*AlwaysInlineOnly=*/true);
   }
 };
}

 char AlwaysInlinerLegacyPass::ID = 0;
 INITIALIZE_PASS_BEGIN(AlwaysInlinerLegacyPass, "alwaysinline",
                       "Inliner for always_inline functions", false, false)
 INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
 INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
 INITIALIZE_PASS_DEPENDENCY(ProfileSummaryInfoWrapperPass)
 INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
 INITIALIZE_PASS_END(AlwaysInlinerLegacyPass, "alwaysinline",
                     "Inliner for always_inline functions", false, false)
 
 Pass *llvm::createAlwaysInlinerLegacyPass(bool InsertLifetime) {
   return new AlwaysInlinerLegacyPass(InsertLifetime);
 }

/* 
 Get the inline cost for the always-inliner.
 
 The always inliner *only* handles functions which are marked with the
 attribute to force inlining. As such, it is dramatically simpler and avoids
 using the powerful (but expensive) inline cost analysis. Instead it uses
 a very simple and boring direct walk of the instructions looking for
 impossible-to-inline constructs.
 
 Note, it would be possible to go to some lengths to cache the information
 computed here, but as we only expect to do this for relatively few and
 small functions which have the explicit attribute to force inlining, it is
 likely not worth it in practice.
*/


std::map<std::string,bool> cachedDecendants;

std::set<Function*> getParents(Function* F) {
  std::set<Function*> parentSet;  
  for (User *U : F->users()) {
    if (auto *CB = dyn_cast<CallBase>(U)) {
      Function* Caller = CB->getParent()->getParent();
      parentSet.insert(Caller);
    }
  }
  return parentSet;
}

bool canAccelerate(Function *F) {
  auto F_name = F->getName().str();
  auto env_dec_accelerators = getenv("DECADES_ACCELERATORS");
  int dec_acc = 0;
  if (env_dec_accelerators) {
    if (atoi(env_dec_accelerators)) {
      dec_acc = 1;
    }
  }
  
  if (dec_acc) {
    if (std::find(accelerators.begin(), accelerators.end(), F_name) != accelerators.end()) {
      return true;
    }
  }

  return false;
}


bool isDecadesPrimitive(Function* F) { 
  auto F_name = F->getName().str();
  if (F_name.find("decades_") != std::string::npos) {
    return true;
  }
  if (F_name.find("dec_") != std::string::npos) {
    return true;
  }
  if (F_name.find("DECADES_") != std::string::npos) {
    return true;
  }
  if (F_name.find("compute_exclusive") != std::string::npos) {
    return true;
  }
  if (F_name.find("supply_exclusive") != std::string::npos) {
    return true;
  }
  if (F_name.find("compute_side") != std::string::npos) {
    return true;
  }

  return false;
}

std::set<std::string> checkRecursion;
bool isKernelDescendant(Function* F, std::set<std::string> parents) {

  auto F_name = F->getName().str();
    
  if (cachedDecendants.find(F_name) != cachedDecendants.end()) {
    return cachedDecendants[F_name];
  }

  if (parents.find(F_name) != parents.end()) {
    cachedDecendants[F_name] = false;
    return false;
  }


  if (F_name.find("_libc_start_main")!= std::string::npos) {
    cachedDecendants[F_name] = false;
    return false;
  }
  
  std::set<Function*> parentSet = getParents(F);
  for (auto* Parent: parentSet) {
    if (isKernelFunction(*Parent)) {
      cachedDecendants[F_name] = true;
      return true;
    }
    else {
      parents.insert(F_name);
      if (isKernelDescendant(Parent, parents)) {
	cachedDecendants[F_name] = true;
	return true;
      }
    }
  }
  cachedDecendants[F_name] = false;
  return false;  
}

InlineCost AlwaysInlinerLegacyPass::getInlineCost(CallBase &CS) {
 
  Function *Callee = CS.getCalledFunction();
  
  if (canAccelerate(Callee)) {
    return InlineCost::getNever("");
  }

  /*
  if(isKernelDescendant(Callee)) {
    errs() << "KERNEL DESCENDANT: " << Callee->getName() << "\n";
  }
  else {
    errs() << "NOT DESCENDANT: " << Callee->getName() << "\n";

  }
  */
   // Only inline direct calls to functions with always-inline attributes
   // that are viable for inlining. FIXME: We shouldn't even get here for
   // declarations.
  
  checkRecursion.clear();
  if (isDecadesPrimitive(Callee)) {
    return InlineCost::getNever("");
  }
  
  bool isKD = isKernelDescendant(Callee, checkRecursion);
  
  if (Callee && !Callee->isDeclaration() /*&& isInlineViable(*Callee)*/ && isKD) {
    if (Callee->getName().str().find("NRT") == std::string::npos) {
      //errs() << "inlining: " << Callee->getName() << "\n";
      return InlineCost::getAlways("");
    }
  }
  else if (isKD) {
    errs() << "WARNING: unable to inline: " << Callee->getName() << "\n";
  }
  return InlineCost::getNever("");
}

//char AlwaysInlinerLegacyPass::ID = 0;
static RegisterPass<AlwaysInlinerLegacyPass> rp("alwaysinline", "Pass to always inline", false, true);
StringRef AlwaysInlinerLegacyPass::getPassName() const {
  return "Always Inline Pass";
}

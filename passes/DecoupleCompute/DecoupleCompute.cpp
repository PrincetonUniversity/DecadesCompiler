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

  #include "../DecoupleCommon/Decouple.h"
  std::map<std::string, int> unsupported_load_types;

  int LOADS = 0;
  int STORES = 0;
  int NON_DEC_STORES = 0;
  int INT_LABEL = 0;

  Function *compute_consume_i8;
  Function *compute_consume_i32;
  Function *compute_consume_i64;
  Function *compute_consume_ptr;

  Function *compute_consume_cmp;

  Function *compute_consume_float;
  Function *compute_consume_double;

  Function *compute_produce_i8;
  Function *compute_produce_i32;
  Function *compute_produce_i64;
  Function *compute_produce_ptr;

  Function *compute_produce_float;
  Function *compute_produce_double;

  Function *compute_consume__Znwm;

  Function *compute_consume__Znam;

  vector<Instruction *> to_erase;

  void populate_functions(Module &M) {
    
    compute_consume_i8 = (Function *) M.getOrInsertFunction("desc_compute_consume_i8",
								 FunctionType::get(Type::getInt8Ty(M.getContext()),
										   Type::getInt32Ty(M.getContext()))).getCallee();
    
    compute_consume_i32 = (Function *) M.getOrInsertFunction("desc_compute_consume_i32",
							      FunctionType::get(Type::getInt32Ty(M.getContext()),
										Type::getInt32Ty(M.getContext()))).getCallee();
    
    compute_consume_i64 = (Function *) M.getOrInsertFunction("desc_compute_consume_i64",
							     FunctionType::get(Type::getInt64Ty(M.getContext()),
									       Type::getInt32Ty(M.getContext()))).getCallee();
    
    // using *i8 and will cast later
    compute_consume_ptr = (Function *) M.getOrInsertFunction("desc_compute_consume_ptr",
							     FunctionType::get(Type::getInt8PtrTy(M.getContext()),
									       Type::getInt32Ty(M.getContext()))).getCallee();
    
    compute_consume_float = (Function *) M.getOrInsertFunction("desc_compute_consume_float",
							       FunctionType::get(Type::getFloatTy(M.getContext()),
										 Type::getInt32Ty(M.getContext()))).getCallee();
    
    compute_consume_double = (Function *) M.getOrInsertFunction("desc_compute_consume_double",
								 FunctionType::get(Type::getDoubleTy(M.getContext()),
										   Type::getInt32Ty(M.getContext()))).getCallee();

    compute_produce_i8 = (Function *) M.getOrInsertFunction("desc_compute_produce_i8",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getInt8Ty(M.getContext()),
									      Type::getInt32Ty(M.getContext()))).getCallee();
    
    compute_produce_i32 = (Function *) M.getOrInsertFunction("desc_compute_produce_i32",
							     FunctionType::get(Type::getVoidTy(M.getContext()),
									       Type::getInt32Ty(M.getContext()),
									       Type::getInt32Ty(M.getContext()))).getCallee();
    
    compute_produce_i64 = (Function *) M.getOrInsertFunction("desc_compute_produce_i64",
							     FunctionType::get(Type::getVoidTy(M.getContext()),
									       Type::getInt64Ty(M.getContext()),
									       Type::getInt32Ty(M.getContext()))).getCallee();

    // using *i8 and will cast later
    compute_produce_ptr = (Function *) M.getOrInsertFunction("desc_compute_produce_ptr",
							     FunctionType::get(Type::getVoidTy(M.getContext()),
									       Type::getInt8PtrTy(M.getContext()),
									       Type::getInt32Ty(M.getContext()))).getCallee();

    compute_produce_float = (Function *) M.getOrInsertFunction("desc_compute_produce_float",
							     FunctionType::get(Type::getVoidTy(M.getContext()),
									       Type::getFloatTy(M.getContext()),
									       Type::getInt32Ty(M.getContext()))).getCallee();
    
    compute_produce_double = (Function *) M.getOrInsertFunction("desc_compute_produce_double",
								FunctionType::get(Type::getVoidTy(M.getContext()),
										  Type::getDoubleTy(M.getContext()),
										  Type::getInt32Ty(M.getContext()))).getCallee();

    compute_consume_cmp =  (Function *) M.getOrInsertFunction("desc_compute_consume_cmp",
							      FunctionType::get(Type::getInt8Ty(M.getContext()),
										Type::getInt32Ty(M.getContext()))).getCallee();

    compute_consume__Znwm = (Function *) M.getOrInsertFunction("desc_compute_consume__Znwm",
							       FunctionType::get(Type::getInt8PtrTy(M.getContext()),
										 Type::getInt64Ty(M.getContext()),
										 Type::getInt32Ty(M.getContext()))).getCallee();

    compute_consume__Znam = (Function *) M.getOrInsertFunction("desc_compute_consume__Znam",
							       FunctionType::get(Type::getInt8PtrTy(M.getContext()),
										 Type::getInt64Ty(M.getContext()),
										 Type::getInt32Ty(M.getContext()))).getCallee();
    
    
  }

  Function *get_non_ptr_consume(Instruction *I) {
    if (I->getType()->isIntegerTy(8)) {
      return compute_consume_i8;
    }
    else if (I->getType()->isIntegerTy(32)) {
      return compute_consume_i32;
    }
    else if (I->getType()->isIntegerTy(64)) {
      return compute_consume_i64;
    }
    else if (I->getType()->isFloatTy()) {
      return compute_consume_float;
    }
    else if (I->getType()->isDoubleTy()) {
      return compute_consume_double;
    }
    else {
      errs() << "[Error: unsupported non-pointer type]\n";
      errs() << *I << "\n";
      errs() << *(I->getType()) << "\n";
      assert(0);      
    }	
  }

  void instrument_load(Module &M, Instruction *I) {
    LOADS++;
    Instruction *Intr;
    Value *Intr2;
    IRBuilder<> Builder(I);

    if (non_ptr_load(I)) {
      Function *compute_consume_non_ptr_func = get_non_ptr_consume(I);
      Intr = Builder.CreateCall(compute_consume_non_ptr_func, {ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      I->replaceAllUsesWith(Intr);
      to_erase.push_back(I);
      INT_LABEL++;
    }
    else if (ptr_load(I)) {
      Intr = Builder.CreateCall(compute_consume_ptr, {ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});      
      Intr2 = Builder.CreatePointerCast(Intr, I->getType());
      I->replaceAllUsesWith(Intr2);
      to_erase.push_back(I);
      INT_LABEL++;
    }
    else {
      std::string type_str;
      llvm::raw_string_ostream rso(type_str);
      I->getType()->print(rso);
      auto my_str = rso.str();
      if (unsupported_load_types.find(my_str) == unsupported_load_types.end()) {
	unsupported_load_types[my_str] = 0;
      }
      unsupported_load_types[my_str]++;
      //errs() << "[Warning: Could not find a type for the load instruction]\n";
      //errs() << *I << "\n";
      //errs() << *(I->getType()) << "\n";
      //assert(0);      
    }
  }


  Function *get_non_ptr_produce(StoreInst *stI) {
    Value *I = stI->getValueOperand();
    if (I->getType()->isIntegerTy(8)) {
      return compute_produce_i8;
    }
    else if (I->getType()->isIntegerTy(32)) {
      return compute_produce_i32;
    }
    else if (I->getType()->isIntegerTy(64)) {
      return compute_produce_i64;
    }
    else if (I->getType()->isFloatTy()) {
      return compute_produce_float;
    }
    else if (I->getType()->isDoubleTy()) {
      return compute_produce_double;
    }

    else {
      errs() << "[Error: unsupported non-pointer type]\n";
      errs() << *I << "\n";
      errs() << *(I->getType()) << "\n";
      assert(0);      
    }	
  }


  void instrument_store(Module &M, StoreInst *I) {
    STORES++;
    Instruction *Intr;
    Value *Intr2;
    IRBuilder<> Builder(I);

    if (non_ptr_store(I)) {
      Function *compute_produce_non_ptr_func = get_non_ptr_produce(I);
      Intr = Builder.CreateCall(compute_produce_non_ptr_func, {I->getValueOperand(), {ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)}});
      to_erase.push_back(I);
      INT_LABEL++;
    }
    else if (ptr_store(I)) {      
      // This gets really nasty. Just going to have supply take care of pointer stores.

      if (safe_ptr_store_common(I->getValueOperand())) {
	
	Intr2 = Builder.CreatePointerCast(I->getValueOperand(), Type::getInt8PtrTy(M.getContext()));
	Intr = Builder.CreateCall(compute_produce_ptr,{Intr2, ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
	INT_LABEL++;
      }
      else {
	NON_DEC_STORES++;
      }
      
      //Intr2 = Builder.CreatePointerCast(I->getValueOperand(), Type::getInt8PtrTy(M.getContext()));
      //Intr = Builder.CreateCall(compute_produce_ptr,{Intr2});
	
      to_erase.push_back(I);      
    }
    else {
      std::string type_str;
      llvm::raw_string_ostream rso(type_str);
      I->getType()->print(rso);
      auto my_str = rso.str();
      if (unsupported_load_types.find(my_str) == unsupported_load_types.end()) {
	unsupported_load_types[my_str] = 0;
      }
      unsupported_load_types[my_str]++;
      //to_erase.push_back(I);
    }
  }

  bool try_instrument_function(Module &M, CallInst *I) {
    if (I->getCalledFunction() == NULL) {
      return false;
    }
    if (I->getCalledFunction()->getName().str() == "_Znwm") {
      IRBuilder<> Builder(I);
      //errs() << *I << "\n\n";
      //errs() << *I->getOperand(0) << "\n\n";

      Instruction *Intr = Builder.CreateCall(
					     compute_consume__Znwm,
					     {I->getOperand(0),
					      ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      
      INT_LABEL++;
      I->replaceAllUsesWith(Intr);
      to_erase.push_back(I);
      return true;
    }

    if (I->getCalledFunction()->getName().str() == "_Znam") {
      IRBuilder<> Builder(I);
      Instruction *Intr = Builder.CreateCall(
					     compute_consume__Znam,
					     {I->getOperand(0),
					      ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      
      INT_LABEL++;
      I->replaceAllUsesWith(Intr);
      to_erase.push_back(I);
      return true;
    }

    return false;
  }

  std::set<std::string> to_print;

  void try_delete_mem_functions(Module &M, CallInst *I) {
    /*if (I->getType()->isVoidTy()) {
      //errs() << *(I->getType()) << "\n";
      to_erase.push_back(I);
      }*/



    // Annoying indirect function calls
    if (I->getCalledFunction() == NULL) {
      return;
    }
    
    if (to_print.find(I->getCalledFunction()->getName().str()) == to_print.end()) {
      to_print.insert(I->getCalledFunction()->getName().str());
      //errs() << *I << "\n";
    }

    if (I->getCalledFunction()->getName().str().find("llvm.memcpy") != std::string::npos) {
      to_erase.push_back(I);
      return;
    }
    else if (I->getCalledFunction()->getName().str().find("llvm.memset") != std::string::npos) {
      to_erase.push_back(I);
      return;
    }
    else if (I->getCalledFunction()->getName().str().find("llvm.memmove") != std::string::npos) {
      to_erase.push_back(I);
      return;
    }
    
    else if (I->getCalledFunction()->getName().str().find("__assert_fail") != std::string::npos) {
      to_erase.push_back(I);
      return;
    }
    //else if (I->getCalledFunction()->getName().str().find("NRT_Free") != std::string::npos) {
    //  to_erase.push_back(I);
    //  return;
    //}    

    else if (I->getCalledFunction()->getName().str().find("_ZN") != std::string::npos ||
	     I->getCalledFunction()->getName().str().find("_Zd") != std::string::npos ||
             I->getCalledFunction()->getName().str().find("_ZS") != std::string::npos)
      {
      if (I->getType()->isVoidTy()) {
	to_erase.push_back(I);
	return;
      }
      else if (I->getNumUses() == 0) {
	to_erase.push_back(I);
	return;
      }
      //
      //errs() << I->getNumUses() << "\n";
    }
    //errs() << *I << "\n";
  }
  
  void deal_with_cmp(Module &M, ICmpInst *I) {
    Value *V1 = I->getOperand(0);
    Value *V2 = I->getOperand(1);
    if (V1->getType()->isPointerTy() || V2->getType()->isPointerTy()) {
      IRBuilder<> Builder(I);
      Value * Intr = Builder.CreateCall(compute_consume_cmp, {ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      Value * V = Builder.CreateIntCast(Intr, Type::getInt1Ty(M.getContext()), true);

      I->replaceAllUsesWith(V);
      to_erase.push_back(I);
      INT_LABEL++;
    }
    return;
  }

  void check_rmw(Module &M, CallInst *ci) {
    IRBuilder<> Builder(ci);
    Instruction* Intr;
    if (ci->getCalledFunction()->getName().str().find("DECADES_FETCH_ADD_FLOAT") != std::string::npos) {
      Intr = Builder.CreateCall(compute_consume_float, {ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      INT_LABEL++;
      ci->replaceAllUsesWith(Intr);
      to_erase.push_back(ci);
    } else if (isRMW(ci)) {
      Intr = Builder.CreateCall(compute_consume_i32, {ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      INT_LABEL++;
      ci->replaceAllUsesWith(Intr);
      to_erase.push_back(ci);
    }
  }

  void instrument_compute(Module &M, Function &f) {
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa<LoadInst>(*iI)) {
	instrument_load(M, (&(*iI)));
      }
      else if (isa<ExtractValueInst>(*iI)) {
	if (is_decoupled_value(&(*iI))) {
	  ExtractValueInst *tmp = dyn_cast<ExtractValueInst>(&(*iI));
	  instrument_load(M,(&(*iI)));	  
	}
      }
      else if (isa<StoreInst>(*iI)) {
	instrument_store(M, dyn_cast<StoreInst>(&(*iI)));
      }
      else if (isa<CallInst>(*iI)) {	
	CallInst *tmp = dyn_cast<CallInst>(&(*iI));

	//For an RMW we need to change this to a consume
	check_rmw(M, dyn_cast<CallInst>(&(*iI)));
	
	if (!try_instrument_function(M, dyn_cast<CallInst>(&(*iI)))) {	  	  
	  try_delete_mem_functions(M, dyn_cast<CallInst>(&(*iI)));
	}
      }
      else if (isa<ICmpInst>(*iI)) {
	deal_with_cmp(M, dyn_cast<ICmpInst>(&(*iI)));
      }
    }
    
    for (int i = 0; i < to_erase.size(); i++) {
      to_erase[i]->eraseFromParent();
    }
  }


  struct DecoupleComputePass : public ModulePass {
    static char ID;
    DecoupleComputePass() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
      errs() << "[Decouple Compute Pass Begin]\n";

      populate_functions(M);
      populate_functions_common(M);


      for (Module::iterator fI = M.begin(), fE = M.end(); fI != fE; ++fI) {
	if (isComputeKernelFunction(*fI)) {
	  errs() << "[Found Kernel]\n";	  
	  errs() << "[" << fI->getName().str() << "]\n";
	  instrument_compute(M, *fI);
	}
      }

      errs() << "[Unsupported memory access types]\n";
      
      for ( const auto &myPair : unsupported_load_types) {
	errs() << myPair.first << " : " << myPair.second << "\n";
      }
      

      errs() << "[STORES: " << STORES << "]\n";
      errs() << "[NON_DEC_STORES: " << NON_DEC_STORES << "]\n";
      errs() << "[LOADS:  " << LOADS << "]\n";
      errs() << "[Decouple Compute Pass Finished]\n";
      return true;
    }
  };
}

char DecoupleComputePass::ID = 0;
static RegisterPass<DecoupleComputePass> X("decouplecompute", "Decouple Compute Pass", false, false);

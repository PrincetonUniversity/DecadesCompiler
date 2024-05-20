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
  std::map<std::string, int> unsupported_load_types;

  int LOADS = 0;
  int STORES = 0;
  int NON_DEC_STORES = 0;
  int INT_LABEL = 0;

  Function *dec_open_producer;
  Function *dec_close_producer;
  Function *dec_produce32;
  Function *dec_produce64;
  
  Function *compute_exclusive_store_uint8;
  Function *compute_exclusive_store;
  Function *compute_exclusive_store_long;
  Function *compute_exclusive_store_float;
  Function *compute_exclusive_store_double;
  Function *compute_exclusive_store_ptr;

  
  Function *supply_consume_i8;
  Function *supply_consume_i32;
  Function *supply_consume_i64;
  Function *supply_consume_ptr;

  Function *supply_consume_float;
  Function *supply_consume_double;


  Function *supply_produce_i8;
  Function *supply_produce_i32;
  Function *supply_produce_i64;
  Function *supply_produce_ptr;

  Function *supply_produce_float;
  Function *supply_produce_double;

  Function *supply_special_produce_cmp;
  Function *supply_special_produce__Znwm;
  Function *supply_special_produce__Znam;




  vector<Instruction *> to_erase;

  void populate_functions(Module &M) {
    
    dec_open_producer = (Function *) M.getOrInsertFunction("dec_open_producer",
							   FunctionType::get(Type::getInt32Ty(M.getContext()),
									     Type::getInt64Ty(M.getContext()))).getCallee();

    dec_close_producer = (Function *) M.getOrInsertFunction("dec_close_producer",
							   FunctionType::get(Type::getInt32Ty(M.getContext()),
									     Type::getInt64Ty(M.getContext()))).getCallee();

    dec_produce32 = (Function *) M.getOrInsertFunction("dec_produce32",
							   FunctionType::get(Type::getVoidTy(M.getContext()),
									     Type::getInt64Ty(M.getContext()),
									     Type::getInt32Ty(M.getContext()))).getCallee();


    dec_produce64 = (Function *) M.getOrInsertFunction("dec_produce64",
							   FunctionType::get(Type::getVoidTy(M.getContext()),
									     Type::getInt64Ty(M.getContext()),
									     Type::getInt64Ty(M.getContext()))).getCallee();

    compute_exclusive_store_uint8 = (Function *) M.getOrInsertFunction("compute_exclusive_store_uint8",
								       FunctionType::get(Type::getVoidTy(M.getContext()),
											 Type::getInt8PtrTy(M.getContext()),
											 Type::getInt8Ty(M.getContext()))).getCallee();

    compute_exclusive_store = (Function *) M.getOrInsertFunction("compute_exclusive_store",
								 FunctionType::get(Type::getVoidTy(M.getContext()),
										   Type::getInt32PtrTy(M.getContext()),
										   Type::getInt32Ty(M.getContext()))).getCallee();
    
    compute_exclusive_store_ptr = (Function *) M.getOrInsertFunction("compute_exclusive_store_ptr",
								 FunctionType::get(Type::getVoidTy(M.getContext()),
										   Type::getInt8PtrTy(M.getContext())->getPointerTo(),
										   Type::getInt8PtrTy(M.getContext()))).getCallee();
    
    compute_exclusive_store_long = (Function *) M.getOrInsertFunction("compute_exclusive_store_long",
								       FunctionType::get(Type::getVoidTy(M.getContext()),
											 Type::getInt64PtrTy(M.getContext()),
											 Type::getInt64Ty(M.getContext()))).getCallee();

    compute_exclusive_store_float = (Function *) M.getOrInsertFunction("compute_exclusive_store_float",
								       FunctionType::get(Type::getVoidTy(M.getContext()),
											 Type::getFloatPtrTy(M.getContext()),
											 Type::getFloatTy(M.getContext()))).getCallee();

    compute_exclusive_store_double = (Function *) M.getOrInsertFunction("compute_exclusive_store_double",
								       FunctionType::get(Type::getVoidTy(M.getContext()),
											 Type::getDoublePtrTy(M.getContext()),
											 Type::getDoubleTy(M.getContext()))).getCallee();


    
    
    // ***************************************
    // DeSC functions:
    // **************************************

    
    supply_consume_i8 = (Function *) M.getOrInsertFunction("desc_supply_consume_i8",
							   FunctionType::get(Type::getVoidTy(M.getContext()),
									     Type::getInt8PtrTy(M.getContext()),
									     Type::getInt32Ty(M.getContext()))).getCallee();
    
    supply_consume_i32 = (Function *) M.getOrInsertFunction("desc_supply_consume_i32",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getInt32PtrTy(M.getContext()),
									      Type::getInt32Ty(M.getContext()))).getCallee();
    
    supply_consume_i64 = (Function *) M.getOrInsertFunction("desc_supply_consume_i64",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getInt64PtrTy(M.getContext()),
									      Type::getInt32Ty(M.getContext()))).getCallee();
    
    // using *i8 and will cast later
    supply_consume_ptr = (Function *) M.getOrInsertFunction("desc_supply_consume_ptr",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getInt8PtrTy(M.getContext())->getPointerTo(),
									      Type::getInt32Ty(M.getContext()))).getCallee();

    supply_consume_float = (Function *) M.getOrInsertFunction("desc_supply_consume_float",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getFloatPtrTy(M.getContext()),
									      Type::getInt32Ty(M.getContext()))).getCallee();
    
    supply_consume_double = (Function *) M.getOrInsertFunction("desc_supply_consume_double",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getDoublePtrTy(M.getContext()),
									      Type::getInt32Ty(M.getContext()))).getCallee();

    
    supply_produce_i8 = (Function *) M.getOrInsertFunction("desc_supply_produce_i8",
							   FunctionType::get(Type::getVoidTy(M.getContext()),
									     Type::getInt8Ty(M.getContext()),
									     Type::getInt32Ty(M.getContext()))).getCallee();
    
    supply_produce_i32 = (Function *) M.getOrInsertFunction("desc_supply_produce_i32",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getInt32Ty(M.getContext()),
									      Type::getInt32Ty(M.getContext()))).getCallee();
    
    supply_produce_i64 = (Function *) M.getOrInsertFunction("desc_supply_produce_i64",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getInt64Ty(M.getContext()),
									      Type::getInt32Ty(M.getContext()))).getCallee();
    
    // using *i8 and will cast later
    /*supply_produce_ptr = (Function *) M.getOrInsertFunction("desc_supply_produce_ptr",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getInt8PtrTy(M.getContext()),
									      Type::getInt32Ty(M.getContext())));*/

    supply_produce_ptr = (Function *) M.getOrInsertFunction("desc_supply_produce_ptr",
    							    FunctionType::get(Type::getVoidTy(M.getContext()),
    									      Type::getInt8PtrTy(M.getContext()),Type::getInt32Ty(M.getContext())
    									      )).getCallee();

    supply_produce_float = (Function *) M.getOrInsertFunction("desc_supply_produce_float",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getFloatTy(M.getContext()),
									      Type::getInt32Ty(M.getContext()))).getCallee();
    
    supply_produce_double = (Function *) M.getOrInsertFunction("desc_supply_produce_double",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getDoubleTy(M.getContext()),
									      Type::getInt32Ty(M.getContext()))).getCallee();

    supply_special_produce_cmp = (Function *) M.getOrInsertFunction("desc_supply_special_produce_cmp",
							       FunctionType::get(Type::getVoidTy(M.getContext()),
										 Type::getInt8Ty(M.getContext()), Type::getInt32Ty(M.getContext()))).getCallee();

    supply_special_produce__Znwm = (Function *) M.getOrInsertFunction("desc_special_supply_produce__Znwm",
							       FunctionType::get(Type::getVoidTy(M.getContext()),
										 Type::getInt8Ty(M.getContext()), Type::getInt32Ty(M.getContext()))).getCallee();


    supply_special_produce__Znam = (Function *) M.getOrInsertFunction("desc_special_supply_produce__Znam",
								      FunctionType::get(Type::getVoidTy(M.getContext()),
											Type::getInt8Ty(M.getContext()), Type::getInt32Ty(M.getContext()))).getCallee();


    
   
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
  
  Function *get_non_ptr_produce(Instruction *I) {
    if (I->getType()->isIntegerTy(8)) {
      return supply_produce_i8;
    }
    else if (I->getType()->isIntegerTy(32)) {
      return supply_produce_i32;
    }
    else if (I->getType()->isIntegerTy(64)) {
      return supply_produce_i64;
    }
    else if (I->getType()->isFloatTy()) {
      return supply_produce_float;
    }
    else if (I->getType()->isDoubleTy()) {
      return supply_produce_double;
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
    Value *Intr_pre;
    IRBuilder<> Builder(I->getNextNode());

    if (non_ptr_load(I)) {
      Function *supply_produce_non_ptr_func = get_non_ptr_produce(I);
      Intr = Builder.CreateCall(supply_produce_non_ptr_func, {I, ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      INT_LABEL++;
    }
    else if (ptr_load(I)) {
      // No casting actually needed
      Intr_pre = Builder.CreatePointerCast(I, Type::getInt8PtrTy(M.getContext()));
      Intr = Builder.CreateCall(supply_produce_ptr, {Intr_pre, ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
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
    }
  }


  Function *get_non_ptr_consume(StoreInst *stI) {
    Value *I = stI->getValueOperand();
    if (I->getType()->isIntegerTy(8)) {
      return supply_consume_i8;
    }
    else if (I->getType()->isIntegerTy(32)) {
      return supply_consume_i32;
    }
    else if (I->getType()->isIntegerTy(64)) {
      return supply_consume_i64;
    }
    else if (I->getType()->isFloatTy()) {
      return supply_consume_float;
    }
    else if (I->getType()->isDoubleTy()) {
      return supply_consume_double;
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
      Function *supply_consume_non_ptr_func = get_non_ptr_consume(I);
      Intr = Builder.CreateCall(supply_consume_non_ptr_func, {I->getPointerOperand(), ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      to_erase.push_back(I);
      INT_LABEL++;
    }
    else if (ptr_store(I)) {
      if (safe_ptr_store_common(I->getValueOperand())) {
	
	Intr2 = Builder.CreatePointerCast(I->getPointerOperand(), Type::getInt8PtrTy(M.getContext())->getPointerTo());
	Intr = Builder.CreateCall(supply_consume_ptr,{Intr2, ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
	to_erase.push_back(I);
	INT_LABEL++;
      }
      else {
	NON_DEC_STORES++;
      }
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

    }
  }

  void deal_with_cmp(Module &M, ICmpInst *ICI) {
    Value *V1 = ICI->getOperand(0);
    Value *V2 = ICI->getOperand(1);
    if (V1->getType()->isPointerTy() || V2->getType()->isPointerTy()) {
      IRBuilder<> Builder(ICI->getNextNode());
      Value * V = Builder.CreateIntCast(ICI, Type::getInt8Ty(M.getContext()), true);
      Builder.CreateCall(supply_special_produce_cmp, {V, ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      
      INT_LABEL++;
    }
    return;
  }

  void try_instrument_function_supply(Module &M, CallInst *I) {
    if (I->getCalledFunction() == NULL) {
      return;
    }
    if (I->getCalledFunction()->getName().str() == "_Znwm") {
      IRBuilder<> Builder(I->getNextNode());
      Builder.CreateCall(supply_special_produce__Znwm, {I, ConstantInt::get(Type::getInt32Ty(M.getContext()), INT_LABEL)});
      INT_LABEL++;
      
    }
    if (I->getCalledFunction()->getName().str() == "_Znam") {
      IRBuilder<> Builder(I->getNextNode());
      Builder.CreateCall(supply_special_produce__Znam, {I, ConstantInt::get(Type::getInt32Ty(M.getContext()), INT_LABEL)});
      INT_LABEL++;      
    }
  }

  void check_rmw(Module &M, CallInst *ci) {
    IRBuilder<> Builder(ci->getNextNode());
    Instruction* Intr;
    if (ci->getCalledFunction()->getName().str().find("dec_atomic_fetch_add_float") != std::string::npos) {
      Intr = Builder.CreateCall(supply_produce_float, {ci, ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      INT_LABEL++;
      //errs() << INT_LABEL;
    } else if (isRMW(ci)) {
      Intr = Builder.CreateCall(supply_produce_i32, {ci, ConstantInt::get(Type::getInt32Ty(M.getContext()),INT_LABEL)});
      INT_LABEL++;
      //errs() << INT_LABEL;
    }
  }

  void instrument_supply(Module &M, Function &f) {
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {

      if (isa<LoadInst>(*iI)) {
	instrument_load(M, &(*iI));
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
      else if (isa<ICmpInst>(*iI)) {
	deal_with_cmp(M, dyn_cast<ICmpInst>(&(*iI)));
      }
      else if (isa<CallInst>(*iI)) {
        check_rmw(M, dyn_cast<CallInst>(&(*iI)));
	try_instrument_function_supply(M, dyn_cast<CallInst>(&(*iI)));
      }
    }
    
    for (int i = 0; i < to_erase.size(); i++) {
      to_erase[i]->eraseFromParent();
    }
    to_erase.clear();
  }

  bool can_delete_inst(Value *val) {
    if (isa<CallInst>(*val)) {
      CallInst * ci = dyn_cast<CallInst>(&(*val));
      if (ci->getCalledFunction()->getName().str().find("desc_supply") != std::string::npos) {
        return false;
      }
    }

    return true;
  }
  
  void delete_uses(Value *val, bool base) {
    /*if (!base && isa<CallInst>(*val)) {
      CallInst * ci = dyn_cast<CallInst>(&(*val));
      if (ci->getCalledFunction()->getName().str().find("compute_exclusive") != std::string::npos) {
        return;
      }
      }*/

    if (!can_delete_inst(val)) {
      errs() << "Cannot slice compute exclusive accesses from supply!\n";
      errs() << "Instruction: " << val << "\n";    
      assert(0);
    }

    
    for (auto use_iter : val->users()) {
      if (isa<Instruction>(*use_iter)) {
        Instruction *I = dyn_cast<Instruction>(&(*use_iter));
	// Can't erase termintors :(
	if (I->isTerminator()) {
	  errs() << "Terminator: " << "\n";
	  errs() << *I << "\n\n";
	}
        delete_uses(I, false);
      }
    }
  
    if (isa<Instruction>(*val)) {
      Instruction *I = dyn_cast<Instruction>(&(*val));
      to_erase.push_back(I);
    }
  }

  Function* getComputeExclusiveStoreFunction(Module &M, StoreInst *stI){
    // list all different compute exclusive stores
    Value *I = stI->getValueOperand();
    if (I->getType()->isIntegerTy(8)) {
      return compute_exclusive_store_uint8;
    }
    else if (I->getType()->isIntegerTy(32)) {
      return compute_exclusive_store;
    }
    else if (I->getType()->isIntegerTy(64)) {
      return compute_exclusive_store_long;
    }
    else if (I->getType()->isFloatTy()) {
      return compute_exclusive_store_float;
    }
    else if (I->getType()->isDoubleTy()) {
      return compute_exclusive_store_double;
    }
    //else if (I->getType()->isIntegerTy(32)) {
    //  return compute_exclusive_store;
    //}
    //else if (I->getType()->isDoubleTy()) {
    //  return compute_exclusive_store_double;
    //}

    else {
      errs() << "[Error: unsupported non-pointer type]\n";
      errs() << *I << "\n";
      errs() << *(I->getType()) << "\n";
      assert(0);      
    }	
  }

  

  void makeStoresComputeExclusive(Module &M, Function &f){
    Instruction *Intr;
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa<StoreInst>(*iI)) {
	errs() << "[Found Store Instruction: " << *(&(*iI)) << " ]\n";
	StoreInst * si = dyn_cast<StoreInst>(&(*iI));
	IRBuilder<> Builder(si);
	if (non_ptr_store(si)){
	  Function *computeExclusiveStore = getComputeExclusiveStoreFunction(M, si);
	  errs() << "[NonPtr Store Instruction for computeExclusive " << *computeExclusiveStore << " ]\n";
	  Intr =  Builder.CreateCall(computeExclusiveStore, {si->getOperand(1), si->getOperand(0)}); 
	  si->replaceAllUsesWith(Intr);
	  to_erase.push_back(si);
	}
	else{
	  Function *computeExclusiveStore = compute_exclusive_store_ptr;
	  errs() << "[Pointer Store Instruction for computeExclusive " << *computeExclusiveStore << " ]\n";
	  Intr =  Builder.CreateCall(computeExclusiveStore, {si->getOperand(1), si->getOperand(0)}); 
	  si->replaceAllUsesWith(Intr);
	  to_erase.push_back(si);
	}
      }
    }
    for (int i = 0; i < to_erase.size(); i++) {
      to_erase[i]->eraseFromParent();
    }
    to_erase.clear();    
  }

  void remove_compute_exclusive_load(Module &M, Function &f) {
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa<CallInst>(*iI)) {
        CallInst * ci = dyn_cast<CallInst>(&(*iI));
        if (ci->getCalledFunction()->getName().str().find("compute_exclusive_load") != std::string::npos) {
          delete_uses(ci, true);
        }
      }
    }

    for (int i = 0; i < to_erase.size(); i++) {
      to_erase[i]->eraseFromParent();
    }

    to_erase.clear();
  }

  void remove_compute_exclusive_store(Module &M, Function &f) {
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa<CallInst>(*iI)) {
        CallInst * ci = dyn_cast<CallInst>(&(*iI));
        if (ci->getCalledFunction()->getName().str().find("compute_exclusive_store") != std::string::npos) {
          to_erase.push_back(ci);
        }
      }
    }

    for (int i = 0; i < to_erase.size(); i++) {
      to_erase[i]->eraseFromParent();
    }
    to_erase.clear();
  }

  void remove_compute_exclusive_fetch_add(Module &M, Function &f) {
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa<CallInst>(*iI)) {
        CallInst * ci = dyn_cast<CallInst>(&(*iI));
        if (ci->getCalledFunction()->getName().str().find("compute_exclusive_fetch_add") != std::string::npos) {
	  //to_erase.push_back(ci);
	  errs() <<"[Found compute_exclusive_fetch_add]\n";
	  delete_uses(ci, true);
	  errs() <<"[Deleted compute_exclusive_fetch_add]\n";
        }
      }
    }
    errs() <<"[Erasing all compute_exclusive_fetch_adds]\n";
    for (int i = 0; i < to_erase.size(); i++) {
      errs() <<"[Trying to erase instruction:" << *(to_erase[i])<<  "]\n";
      to_erase[i]->eraseFromParent();
      errs() <<"[DONE !!!]\n";
      
    }

    to_erase.clear();
    errs() <<"[End of remove_compute_exclusive_fetch_add]\n";
  }

  void remove_compute_exclusive_compare_exchange(Module &M, Function &f) {
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa<CallInst>(*iI)) {
        CallInst * ci = dyn_cast<CallInst>(&(*iI));
        if (ci->getCalledFunction()->getName().str().find("compute_exclusive_compare_exchange") != std::string::npos) {
          delete_uses(ci, true);
        }
      }
    }
    
    for (int i = 0; i < to_erase.size(); i++) {
      to_erase[i]->eraseFromParent();
    }
    
    to_erase.clear();
  }
  
  

  /*void remove_compute_side_store(Module &M, Function &f) {
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa<CallInst>(*iI)) {
        CallInst * ci = dyn_cast<CallInst>(&(*iI));
        if (ci->getCalledFunction()->getName().str().find("compute_side_store") != std::string::npos) {
          to_erase.push_back(ci);
        }
	if (ci->getCalledFunction()->getName().str().find("compute_side_fetch_min") != std::string::npos) {
	  to_erase.push_back(ci);
	  ci->replaceAllUsesWith(ConstantInt::get(Type::getInt32Ty(M.getContext()),0));

	  //Probably should figure out how to do this more robustly, but we hit some terminators
	  //delete_uses(ci, true);
        }

      }
    }

    for (int i = 0; i < to_erase.size(); i++) {
      to_erase[i]->eraseFromParent();
    }

    to_erase.clear();
  }*/

  struct DecoupleSupplyPass : public ModulePass {
    static char ID;
    DecoupleSupplyPass() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
      errs() << "[Decouple Supply Pass Begin]\n";
      auto env_dec_thread_num = getenv("DECADES_THREAD_NUM");
      auto env_dec_consumer_num = getenv("DECADES_NUM_CONSUMERS");
      auto env_dec_decoupling_mode = getenv("DECADES_DECOUPLING_MODE");
      
      int DEC_NUM_THREADS=1;
      int DEC_NUM_CONSUMERS=1;
      char DEC_DECOUPLING_MODE='d';
      
      if (env_dec_thread_num) {
	DEC_NUM_THREADS = atoi(env_dec_thread_num);
      }
      
      if (env_dec_consumer_num) {
	DEC_NUM_CONSUMERS = atoi(env_dec_consumer_num);
      }

      Constant *Arg1 = llvm::ConstantInt::get(M.getContext(), llvm::APInt(/*nbits*/32, DEC_NUM_THREADS, /*bool*/1));
      Constant *Arg2 = llvm::ConstantInt::get(M.getContext(), llvm::APInt(/*nbits*/32, DEC_NUM_CONSUMERS, /*bool*/1));
      

      
      GlobalVariable* decNumThreads = new GlobalVariable(/*Module=*/M, 
							 /*Type=*/Type::getInt32Ty(M.getContext()),
							 /*isConstant=*/false,
							 /*Linkage=*/llvm::GlobalValue::ExternalLinkage,
							 /*Initializer=*/Arg1,
							 /*Name=*/"DEC_NUM_THREADS");
      
      
      
      GlobalVariable* decNumConsumers = new GlobalVariable(/*Module=*/M, 
							   /*Type=*/Type::getInt32Ty(M.getContext()),
							   /*isConstant=*/false,
							   /*Linkage=*/llvm::GlobalValue::ExternalLinkage,
							   /*Initializer=*/Arg2,
							   /*Name=*/"DEC_NUM_CONSUMERS");


      populate_functions(M);
      populate_functions_common(M);

      for (Module::iterator fI = M.begin(), fE = M.end(); fI != fE; ++fI) {
	// make all stores compute_exclusive_store for all kernels
	if (isKernelFunction(*fI)) {
	  errs() << "[Making All Stores Compute Exclusive]\n";
	  makeStoresComputeExclusive(M, *fI);
	}
	// remove compute_exclusive operations from supply kernel
	// subsequently, dead-code elimination removes all computations that
	// are not required for any load operations for supply.
	if (isSupplyKernelFunction(*fI)) {
	  errs() << "[Found Kernel]\n";	  
	  errs() << "[" << fI->getName().str() << "]\n";
	  instrument_supply(M, *fI);
	  errs() << "[Finished instrument supply]\n";
          remove_compute_exclusive_load(M, *fI);
	  errs() << "[Finished remove_compute_exclusive_load]\n";
          remove_compute_exclusive_store(M, *fI);
	  errs() << "[Finished remove_compute_exclusive_store]\n";
	  remove_compute_exclusive_fetch_add(M, *fI);
	  errs() << "[Finished remove_compute_exclusive_fetch_add]\n";
	  remove_compute_exclusive_compare_exchange(M, *fI);
	  errs() << "[Finished remove_compute_exclusive_fetch_add]\n";
	  
        }
      }

      // insert desc_init and desc_cleanup to main function through instrument_main
      // this is the part that will handle paralellization
      errs() << "[Starting module iterator to find main module]\n";
      for (Module::iterator fI = M.begin(), fE = M.end(); fI != fE; ++fI) {
	if (isMain(*fI)) {
	  errs() << "[Found Main]\n";	  
	  errs() << "[" << fI->getName().str() << "]\n";
	  instrument_main(M, *fI);
	}
      }

      errs() << "[STORES: " << STORES << "]\n";
      errs() << "[NON_DEC_STORES: " << NON_DEC_STORES << "]\n";
      errs() << "[LOADS:  " << LOADS << "]\n";
      errs() << "[Decouple Supply Pass Finished]\n";
      return true;
    }
  };
}
char DecoupleSupplyPass::ID = 0;
static RegisterPass<DecoupleSupplyPass> X("decouplesupply", "Decouple Supply Pass", false, false);

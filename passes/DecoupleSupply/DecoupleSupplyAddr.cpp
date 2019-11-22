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

  int LOADS = 0;
  int LOADS_REMOVED = 0;
  int PROMOTED_LOADS = 0;
  int PROMOTED_RMWS = 0;
  int STORES = 0;
  int NON_DEC_STORES = 0;
  int EXCLUSIVE_STORES = 0;
  int EXCLUSIVE_RMWS = 0;

  Function *supply_load_produce_i8;
  Function *supply_load_produce_i32;
  Function *supply_load_produce_i64;
  Function *supply_load_produce_ptr;

  Function *supply_load_produce_float;
  Function *supply_load_produce_double;

  Function *count_load_removed;
  Function *count_exclusive_rmw;

  Function *DECADES_FETCH_ADD;
  Function *supply_alu_rmw_fetchadd_float;
  Function *supply_alu_rmw_fetchmin;
  Function *supply_alu_rmw_cas;

  vector<Instruction *> to_erase;
  vector<int > labels_to_erase;

  void populate_functions(Module &M) {
        
    supply_load_produce_i8 = (Function *) M.getOrInsertFunction("desc_supply_load_produce_i8",
								FunctionType::get(Type::getVoidTy(M.getContext()),
										  Type::getInt8Ty(M.getContext())->getPointerTo(),
										  Type::getInt32Ty(M.getContext()))).getCallee();
    
    supply_load_produce_i32 = (Function *) M.getOrInsertFunction("desc_supply_load_produce_i32",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getInt32Ty(M.getContext())->getPointerTo(),
									      Type::getInt32Ty(M.getContext()))).getCallee();
    
    supply_load_produce_i64 = (Function *) M.getOrInsertFunction("desc_supply_load_produce_i64",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getInt64Ty(M.getContext())->getPointerTo(),
									      Type::getInt32Ty(M.getContext()))).getCallee();
    
    // using *i8 and will cast later
    supply_load_produce_ptr = (Function *) M.getOrInsertFunction("desc_supply_load_produce_ptr",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getInt8PtrTy(M.getContext())->getPointerTo(),
									      Type::getInt32Ty(M.getContext()))).getCallee();

    supply_load_produce_float = (Function *) M.getOrInsertFunction("desc_supply_load_produce_float",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getFloatTy(M.getContext())->getPointerTo(),
									      Type::getInt32Ty(M.getContext()))).getCallee();
    
    supply_load_produce_double = (Function *) M.getOrInsertFunction("desc_supply_load_produce_double",
							    FunctionType::get(Type::getVoidTy(M.getContext()),
									      Type::getDoubleTy(M.getContext())->getPointerTo(),
									      Type::getInt32Ty(M.getContext()))).getCallee();

    count_load_removed = (Function *) M.getOrInsertFunction("count_load_removed", 
                                                            FunctionType::get(Type::getVoidTy(M.getContext()),
                                                                              Type::getInt32Ty(M.getContext()))).getCallee();   

    count_exclusive_rmw = (Function *) M.getOrInsertFunction("count_exclusive_rmw",
                                                            FunctionType::get(Type::getVoidTy(M.getContext()),
                                                                              Type::getInt32Ty(M.getContext()))).getCallee();

    std::vector<Type*> types;

    types.push_back(Type::getInt32PtrTy(M.getContext())->getPointerTo());
    types.push_back(Type::getInt32Ty(M.getContext()));
    DECADES_FETCH_ADD = (Function *) M.getOrInsertFunction("DECADES_FETCH_ADD",
                                                            FunctionType::get(Type::getInt32Ty(M.getContext()), &types[0])).getCallee();

    types.clear();

    types.push_back(Type::getFloatPtrTy(M.getContext())->getPointerTo());
    types.push_back(Type::getFloatTy(M.getContext()));
    types.push_back(Type::getInt32Ty(M.getContext()));
    supply_alu_rmw_fetchadd_float = (Function *) M.getOrInsertFunction("desc_supply_alu_rmw_fetchadd_float",
                                                            FunctionType::get(Type::getVoidTy(M.getContext()), &types[0])).getCallee();
    
    types.clear();

    types.push_back(Type::getInt32PtrTy(M.getContext())->getPointerTo());
    types.push_back(Type::getInt32Ty(M.getContext()));
    types.push_back(Type::getInt32Ty(M.getContext()));
    supply_alu_rmw_fetchmin = (Function *) M.getOrInsertFunction("desc_supply_alu_rmw_fetchmin",
                                                            FunctionType::get(Type::getVoidTy(M.getContext()), &types[0])).getCallee();

    types.clear();

    types.push_back(Type::getInt32PtrTy(M.getContext())->getPointerTo());
    types.push_back(Type::getInt32Ty(M.getContext()));
    types.push_back(Type::getInt32Ty(M.getContext()));
    types.push_back(Type::getInt32Ty(M.getContext()));
    supply_alu_rmw_cas = (Function *) M.getOrInsertFunction("desc_supply_alu_rmw_cas",
                                                            FunctionType::get(Type::getVoidTy(M.getContext()), &types[0])).getCallee();
  }
  
 
  bool isa_supply_produce(Instruction *I) {
    if (isa<CallInst>(I)) {
      CallInst *CI = dyn_cast<CallInst>(I);
      if (CI->getCalledFunction() == NULL) {
	return false;
      }
      if (CI->getCalledFunction()->getName().str().find("desc_supply_produce") != std::string::npos) {
	return true;	
      }      
    }
    return false;
  }

  bool isa_compute_consume(Instruction *I) {

    if (isa<CallInst>(I)) {
      CallInst *CI = dyn_cast<CallInst>(I);
      if (CI->getCalledFunction() == NULL) {
	return false;
      }
      if (CI->getCalledFunction()->getName().str().find("desc_compute_consume") != std::string::npos) {
	return true;	
      }      
    }
    return false;
  }

  bool isa_compute_exclusive(Instruction *I) {
    if (isa<CallInst>(I)) {
      CallInst *CI = dyn_cast<CallInst>(I);
      if (CI->getCalledFunction() == NULL) {
        return false;
      }
      if (CI->getCalledFunction()->getName().str().find("compute_exclusive") != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  LoadInst *get_load_instr(Value *val) {
    if (!val->hasOneUse()) {
      assert(0);
      return NULL;
    }
    if (!isa<Instruction>(val)) {
      assert(0);
      return NULL;
    }
    Instruction *I = dyn_cast<Instruction>(val);

    if (isa<BitCastInst>(I)) {
      BitCastInst *BI = dyn_cast<BitCastInst>(I);
      return get_load_instr(BI->getOperand(0));
    }
    if (isa<LoadInst>(I)) {
      return dyn_cast<LoadInst>(I);
    }
    assert(0);
    return NULL;    
  }

  int can_be_promoted_rec(Value *val) {
    if (!val->hasOneUse()) {
      return 0;
    }
    if (!isa<Instruction>(val)) {
      return 0;
    }
    Instruction *I = dyn_cast<Instruction>(val);
    if (isa<BitCastInst>(I)) {
      BitCastInst *BI = dyn_cast<BitCastInst>(I);
      return can_be_promoted_rec(BI->getOperand(0));
    }
    if (isa<LoadInst>(I)) {
      return 1;
    }
    if (isa<CallInst>(I)) {
      CallInst *CI = dyn_cast<CallInst>(I);
      if (isRMW(CI)) {
        return 2;
      }
    }
    //errs() << "\n\n" << *val << "\n\n";
    return 0;
  }

  Function * get_typed_produce(Type *T) {
    if (T->isIntegerTy(8)) {
      return supply_load_produce_i8;
    }
    else if (T->isIntegerTy(32)) {
      return supply_load_produce_i32;
    }
    else if (T->isIntegerTy(64)) {
      return supply_load_produce_i64;
    }
    else if (T->isFloatTy()) {
      return supply_load_produce_float;
    }
    else if (T->isDoubleTy()) {
      return supply_load_produce_double;
    }
    else {
      errs() << "[Error: unsupported non-pointer type]\n";
      errs() << *T << "\n";
      assert(0);      
    }	
    return NULL;
  }

  void erase_tree(Value *val) {
    if (!val->hasOneUse()) {
      assert(0);
      return;
    }
    if (!isa<Instruction>(val)) {
      assert(0);
      return;
    }
    Instruction *I = dyn_cast<Instruction>(val);    
    if (isa<BitCastInst>(I)) {
      BitCastInst *BI = dyn_cast<BitCastInst>(I);
      to_erase.push_back(I);
      erase_tree(BI->getOperand(0));
      return;
    }
    if (isa<LoadInst>(I)) {
      to_erase.push_back(I);
      return;
    }
    if (isa<CallInst>(I)) {
      CallInst *CI = dyn_cast<CallInst>(I);
      if (isRMW(CI)) {
        to_erase.push_back(I);
        return;
      }
    }
    assert(0);
    return;

  }

  void promote(Module &M, CallInst *CI) {
    LoadInst *LI = get_load_instr(CI->getArgOperand(0));
    Value *addr = LI->getPointerOperand();
    IRBuilder<> Builder(LI);
    if (non_ptr_load(LI)) {
      Function * load_produce = get_typed_produce(LI->getType());
      Builder.CreateCall(load_produce, {addr, CI->getArgOperand(1)});
      to_erase.push_back(CI);
      erase_tree(CI->getArgOperand(0));
      PROMOTED_LOADS++;
    }
    else if (ptr_load(LI)) {
      Function * load_produce_ptr = supply_load_produce_ptr;
      Value * Intr_pre = Builder.CreatePointerCast(addr, Type::getInt8PtrTy(M.getContext())->getPointerTo());      
      Builder.CreateCall(load_produce_ptr,{Intr_pre, CI->getArgOperand(1)});
      to_erase.push_back(CI);
      erase_tree(CI->getArgOperand(0));
      PROMOTED_LOADS++;

    }    
  }

  void promoteRMW(Module &M, CallInst *CI) {
    IRBuilder<> Builder(CI);
    CallInst *rmw_instr = dyn_cast<CallInst>(CI->getArgOperand(0));
    Function *func = rmw_instr->getCalledFunction();
    std::string rmw_name = func->getName().str();
    errs() << rmw_name << "\n";

    vector<Value *> args(rmw_instr->arg_begin(), rmw_instr->arg_end());
    ConstantInt *I32 = dyn_cast<ConstantInt>(&(*(CI->getArgOperand(1))));
    int label = I32->getZExtValue();
    args.push_back(ConstantInt::get(Type::getInt32Ty(M.getContext()), label));
    if (rmw_name.find("DECADES_FETCH_ADD_FLOAT") != std::string::npos) {
        Builder.CreateCall(supply_alu_rmw_fetchadd_float, args);
    } else if (rmw_name.find("DECADES_FETCH_MIN") != std::string::npos) {
        Builder.CreateCall(supply_alu_rmw_fetchmin, args);
    } else if (rmw_name.find("DECADES_COMPARE_AND_SWAP") != std::string::npos) {
        Builder.CreateCall(supply_alu_rmw_cas, args);
    }
    to_erase.push_back(CI);
    erase_tree(CI->getArgOperand(0));
    PROMOTED_RMWS++; 
  }

  bool compute_consume_removed(Value *V) {
    if (V->getNumUses() == 0) {
      return true;
    }
    return false;
  }

  void instrument_compute(Module &M, Function &f) {

    // Probably should have some kind of fixed point recursion
    // but... will implement as needed
    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa_compute_consume(&(*iI))) {
	Value * V = dyn_cast<Value>(&(*iI));
	if (compute_consume_removed(V)) {
	  LOADS_REMOVED++;
	  CallInst *CI = dyn_cast<CallInst>(&(*iI));
	  ConstantInt *I32 = dyn_cast<ConstantInt>(&(*(CI->getArgOperand(0))));
	  int label = I32->getZExtValue();
	  labels_to_erase.push_back(label);
	  to_erase.push_back(CI);
	}
      } else if (isa_compute_exclusive(&(*iI))) {
        CallInst *CI = dyn_cast<CallInst>(&(*iI));
        vector<Value *> args(CI->arg_begin(), CI->arg_end());
        IRBuilder<> Builder(CI);
        if (CI->getCalledFunction()->getName().str().find("compute_exclusive_store") != std::string::npos) {
          EXCLUSIVE_STORES++;
          Builder.CreateStore(args[1], args[0]);
          to_erase.push_back(CI);
        } else if (CI->getCalledFunction()->getName().str().find("compute_exclusive_fetch_add") != std::string::npos) {
          EXCLUSIVE_RMWS++;
          auto I = Builder.CreateCall(DECADES_FETCH_ADD, args);
          Builder.CreateCall(count_exclusive_rmw);
          CI->replaceAllUsesWith(I);
          to_erase.push_back(CI);
        }
      }
    }
    for (int i = 0; i < to_erase.size(); i++) {
      to_erase[i]->eraseFromParent();
    }
  }



  void instrument_supply(Module &M, Function &f) {

    for (inst_iterator iI = inst_begin(&f), iE = inst_end(&f); iI != iE; ++iI) {
      if (isa_supply_produce(&(*iI))) {
	LOADS++;
	CallInst *CI = dyn_cast<CallInst>(&(*iI));
	ConstantInt *I32 = dyn_cast<ConstantInt>(&(*(CI->getArgOperand(1))));
	int label = I32->getZExtValue();
	//errs() << "PRINTING PRODUCE: " << *CI << "\n";
	//errs() << "PRINTING PRODUCE: " << can_be_promoted_rec(CI->getArgOperand(0)) << "\n";
	if (std::find(labels_to_erase.begin(), labels_to_erase.end(), label) != labels_to_erase.end()) {
          IRBuilder<> Builder(CI);
          Builder.CreateCall(count_load_removed, {ConstantInt::get(Type::getInt32Ty(M.getContext()), label)});
	  to_erase.push_back(CI);
	  continue;
	}
        int flag = can_be_promoted_rec(CI->getArgOperand(0));	
	if (flag == 1) { // a terminal load and not an RMW
	  promote(M, CI);
	} else if (flag == 2) {
          promoteRMW(M, CI);     
        }	
      }
    }
    for (int i = 0; i < to_erase.size(); i++) {
      to_erase[i]->eraseFromParent();
    }
  }
  
  struct DecoupleSupplyAddrPass : public ModulePass {
    static char ID;
    DecoupleSupplyAddrPass() : ModulePass(ID) {}

    virtual bool runOnModule(Module &M) {
      errs() << "[Decouple Supply Addr Pass Begin]\n";

      populate_functions(M);
      //populate_functions_common(M);

      for (Module::iterator fI = M.begin(), fE = M.end(); fI != fE; ++fI) {
	if (isComputeKernelFunction(*fI)) {
	  errs() << "[Found Compute Kernel]\n";	  
	  errs() << "[" << fI->getName().str() << "]\n";
	  instrument_compute(M, *fI);
	}
      }

      to_erase.clear();

      for (Module::iterator fI = M.begin(), fE = M.end(); fI != fE; ++fI) {
	if (isSupplyKernelFunction(*fI)) {
	  errs() << "[Found Supply Kernel]\n";	  
	  errs() << "[" << fI->getName().str() << "]\n";
	  instrument_supply(M, *fI);
	}
      }

      errs() << "[LOADS:  " << LOADS << "]\n";
      errs() << "[PROMOTED LOADS:  " << PROMOTED_LOADS << "]\n";
      errs() << "[REMOVED:  " << LOADS_REMOVED << "]\n";
      errs() << "[PROMOTED RMWS: " << PROMOTED_RMWS << "]\n";
      errs() << "[COMPUTE EXCLUSIVE STORES : " << EXCLUSIVE_STORES << "]\n";
      errs() << "[COMPUTE EXCLUSIVE RMWS : " << EXCLUSIVE_RMWS << "]\n";
      errs() << "[Decouple Supply Addr Pass Finished]\n";
      return true;
    }
  };
}

char DecoupleSupplyAddrPass::ID = 0;
static RegisterPass<DecoupleSupplyAddrPass> X("decouplesupplyaddr", "Decouple Supply Addr Pass", false, false);

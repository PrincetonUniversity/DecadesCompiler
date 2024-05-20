#include <vector>
#include <string>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#define KERNEL_STR "_kernel_"
#define COMPUTE_KERNEL_STR "_kernel_compute"
#define SUPPLY_KERNEL_STR "_kernel_supply"
#define MAIN_STR "main"
using namespace std;
using namespace llvm;

Function *desc_setenv;
Function *desc_init;
Function *desc_cleanup;

int DECADES_thread_number = 1;

bool is_decoupled_value(Instruction *I) {
  if (I->getType()->isIntegerTy(8)) {
    return true;
  }
  else if (I->getType()->isIntegerTy(32)) {
    return true;
  }
  else if (I->getType()->isIntegerTy(64)) {
    return true;
  }
  else if (I->getType()->isFloatTy()) {
    return true;
  }
  else if (I->getType()->isDoubleTy()) {
    return true;
  }
  else {
    return false;
  }	
}

bool isSupplyKernelFunction(Function &func) {
  return (func.getName().str().find(SUPPLY_KERNEL_STR) != std::string::npos)
    &&
    (func.getName().str().find("cpython") == std::string::npos); 
}

bool isComputeKernelFunction(Function &func) {
  return (func.getName().str().find(COMPUTE_KERNEL_STR) != std::string::npos)
    &&
    (func.getName().str().find("cpython") == std::string::npos);
}

bool isRMW(CallInst *ci) {
  std::string func_name = ci->getCalledFunction()->getName().str();
  std::string RMW_FUNCTIONS[3] = {"dec_atomic_compare", "dec_atomic_fetch_add", "dec_atomic_fetch_min"};
  for (const auto rmw_function : RMW_FUNCTIONS) {
    if (func_name.find(rmw_function) != std::string::npos) {
      return true;
    }
  }
  return false;
}


bool isMain(Function &func) {
  return (
	  (func.getName().str() == MAIN_STR) ||
	  (
	   (func.getName().str().find("cpython") != std::string::npos) &&
	   (func.getName().str().find("tile_launcher") != std::string::npos)
	   )
	  ); // Needs to be equality here!
}

bool non_ptr_load(Instruction *I) {
  return I->getType()->isIntegerTy(8) || I->getType()->isIntegerTy(32) || I->getType()->isIntegerTy(64) ||
    I->getType()->isFloatTy() || I->getType()->isDoubleTy();
}

bool ptr_load(Instruction *I) {
  return I->getType()->isPointerTy();
}

bool non_ptr_store(StoreInst *stI) {
  Value *I = stI->getValueOperand();
  return I->getType()->isIntegerTy(8) || I->getType()->isIntegerTy(32) || I->getType()->isIntegerTy(64) ||
    I->getType()->isFloatTy() || I->getType()->isDoubleTy();
}

bool ptr_store(StoreInst *stI) {
  Value *I = stI->getValueOperand();
  return I->getType()->isPointerTy();
}

void insert_desc_main_cleanup(Function &f) {
  
  for (inst_iterator iI = inst_begin(f), iE = inst_end(f); iI != iE; ++iI) {
    if (ReturnInst::classof(&(*iI))) {
      Instruction *InsertPoint = (&(*iI));
      IRBuilder<> Builder(InsertPoint);
      Builder.CreateCall(desc_cleanup);
    }
  }
}

void insert_desc_main_init(Module &M, Function &f) {
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
  
  //  if (env_dec_decoupling_mode) {
  //  DEC_DECOUPLING_MODE = (char) atoi(env_dec_decoupling_mode);
  // }

  inst_iterator in = inst_begin(f);
  Instruction *InsertPoint = &(*in);
  IRBuilder<> Builder(InsertPoint);

  
  ConstantInt *Arg1 = Builder.getInt32(DEC_NUM_THREADS);
  ConstantInt *Arg2 = Builder.getInt32(DEC_NUM_CONSUMERS);
  ConstantInt *Arg3 = Builder.getInt8(DEC_DECOUPLING_MODE);

  std::vector<llvm::Value *> putsArgs;
  putsArgs.push_back(Arg1);
  putsArgs.push_back(Arg2);
  putsArgs.push_back(Arg3);

  llvm::ArrayRef<llvm::Value *> argsRef(putsArgs);

  Builder.CreateCall(desc_init, putsArgs);

}  

void instrument_main(Module &M, Function &f) {
  //insert_setenv_main_init(M,f);
  insert_desc_main_init(M, f);
  insert_desc_main_cleanup(f);    
}

void populate_functions_common(Module &M) {

  std::vector<Type*> types;

  types.push_back(Type::getInt32Ty(M.getContext()));
  types.push_back(Type::getInt32Ty(M.getContext()));
  types.push_back(Type::getInt8Ty(M.getContext()));
  
   

  desc_init = (Function *) M.getOrInsertFunction("desc_init",
						 FunctionType::get(Type::getVoidTy(M.getContext()), &types[0])).getCallee();

  desc_cleanup = (Function *) M.getOrInsertFunction("desc_cleanup",
						    FunctionType::get(Type::getVoidTy(M.getContext()), false)).getCallee();

}

// Not 100% sure but if use base is a load then we should be okay
bool safe_ptr_store_common(Value *in_val) {
  if (isa<Constant>(in_val)) {
    return false;
  }
  else if (isa<LoadInst>(in_val)) {
    return true;	
  }
  else if (isa<GetElementPtrInst>(in_val)) {
    GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(in_val);
    Value *val2 = GEP->getPointerOperand();
    return safe_ptr_store_common(val2);
  }
  else if (isa<BitCastInst>(in_val)) {
    BitCastInst *BI = dyn_cast<BitCastInst>(in_val);
    Value *val2 = BI->getOperand(0);
    return safe_ptr_store_common(val2);    
  }
  else if (isa<CallInst>(in_val)) {
    CallInst *CI = dyn_cast<CallInst>(in_val);
    if (CI->getCalledFunction()->getName().str() == "desc_compute_consume_ptr") {
      return true;
    }
  }
  // Could check for Phi nodes and recursively do something with them
  //errs() << "\n" << *in_val << "\n";
  return false;
}

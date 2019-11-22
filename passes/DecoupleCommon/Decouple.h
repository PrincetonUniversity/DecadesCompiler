#define KERNEL_STR "_kernel_"
#define COMPUTE_KERNEL_STR "_kernel_compute"
#define SUPPLY_KERNEL_STR "_kernel_supply"
#define MAIN_STR "main"


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
  std::string RMW_FUNCTIONS[2] = {"DECADES_COMPARE", "DECADES_FETCH"};
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
  if (env_dec_thread_num) {
    DECADES_thread_number = atoi(env_dec_thread_num);
  }

  
  inst_iterator in = inst_begin(f);
  Instruction *InsertPoint = &(*in);
  IRBuilder<> Builder(InsertPoint);
  
  Builder.CreateCall(desc_init, {ConstantInt::get(Type::getInt32Ty(M.getContext()),DECADES_thread_number)});
}  



void instrument_main(Module &M, Function &f) {
  insert_desc_main_init(M, f);
  insert_desc_main_cleanup(f);    
}

void populate_functions_common(Module &M) {

  desc_init = (Function *) M.getOrInsertFunction("desc_init",
						   FunctionType::get(Type::getVoidTy(M.getContext()),Type::getInt32Ty(M.getContext()))).getCallee();
    
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

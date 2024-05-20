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
  Function *compute_exclusive_store_uint8;
  Function *compute_exclusive_store;
  Function *compute_exclusive_store_long;
  Function *compute_exclusive_store_float;
  Function *compute_exclusive_store_double;
  Function *compute_exclusive_store_ptr;
  Function *compute_exclusive_fetch_add;
  Function *compute_exclusive_fetch_min;
  Function *compute_exclusive_compare_exchange;
  Function *compute_exclusive_load;
  Function *compute_exclusive_load_float;
  
  vector<Instruction*> CandidateNMAs;
  vector<Instruction*> LoopEndInstrs;
  vector<Loop*> Loops;
  vector<Instruction *> to_erase;

  set<Instruction*> DependentInstructions;
  
  int NMADepth=0;  
  #include "Decouple.h"
  std::map<std::string, int> seen_functions;

  void populate_functions(Function &F) {
    Module *M = F.getParent();
    
    compute_exclusive_store_uint8 = (Function *) M->getOrInsertFunction("compute_exclusive_store_uint8",
								       FunctionType::get(Type::getVoidTy(M->getContext()),
											 Type::getInt8PtrTy(M->getContext()),
											 Type::getInt8Ty(M->getContext()))).getCallee();

    compute_exclusive_store = (Function *) M->getOrInsertFunction("compute_exclusive_store",
								 FunctionType::get(Type::getVoidTy(M->getContext()),
										   Type::getInt32PtrTy(M->getContext()),
										   Type::getInt32Ty(M->getContext()))).getCallee();
    
    compute_exclusive_store_long = (Function *) M->getOrInsertFunction("compute_exclusive_store_long",
								       FunctionType::get(Type::getVoidTy(M->getContext()),
											 Type::getInt64PtrTy(M->getContext()),
											 Type::getInt64Ty(M->getContext()))).getCallee();

    compute_exclusive_store_float = (Function *) M->getOrInsertFunction("compute_exclusive_store_float",
								       FunctionType::get(Type::getVoidTy(M->getContext()),
											 Type::getFloatPtrTy(M->getContext()),
											 Type::getFloatTy(M->getContext()))).getCallee();

    compute_exclusive_store_double = (Function *) M->getOrInsertFunction("compute_exclusive_store_double",
								       FunctionType::get(Type::getVoidTy(M->getContext()),
											 Type::getDoublePtrTy(M->getContext()),
											 Type::getDoubleTy(M->getContext()))).getCallee();

    
    compute_exclusive_store_ptr = (Function *) M->getOrInsertFunction("compute_exclusive_store_ptr",
								       FunctionType::get(Type::getVoidTy(M->getContext()),
											 Type::getInt8PtrTy(M->getContext())->getPointerTo(),
											 Type::getInt8PtrTy(M->getContext()))).getCallee();

    
    compute_exclusive_load = (Function *) M->getOrInsertFunction("compute_exclusive_load",
								FunctionType::get(Type::getInt32Ty(M->getContext()),
										  Type::getInt32PtrTy(M->getContext())->getPointerTo())).getCallee();
    
    compute_exclusive_load_float = (Function *) M->getOrInsertFunction("compute_exclusive_load_float",
								      FunctionType::get(Type::getFloatTy(M->getContext()),
											Type::getFloatPtrTy(M->getContext())->getPointerTo())).getCallee();
    
    compute_exclusive_fetch_add = (Function *) M->getOrInsertFunction("compute_exclusive_fetch_add",
								     FunctionType::get(Type::getInt32Ty(M->getContext()),
										       Type::getInt32PtrTy(M->getContext()),
										       Type::getInt32Ty(M->getContext()))).getCallee();

    compute_exclusive_fetch_min = (Function *) M->getOrInsertFunction("compute_exclusive_fetch_min",
								     FunctionType::get(Type::getInt32Ty(M->getContext()),
										       Type::getInt32PtrTy(M->getContext())->getPointerTo(),
										       Type::getInt32Ty(M->getContext()))).getCallee();

    std::vector<Type*> types;
    types.clear();
    types.push_back(Type::getInt32PtrTy(M->getContext())->getPointerTo());
    types.push_back(Type::getInt32Ty(M->getContext()));
    types.push_back(Type::getInt32Ty(M->getContext()));

    compute_exclusive_compare_exchange = (Function *) M->getOrInsertFunction("compute_exclusive_compare_exchange",
									     FunctionType::get(Type::getInt32Ty(M->getContext()), &types[0])).getCallee();

  }


  bool isKernelFunction(Function &func) {
    if (
	((func.getName().str().find(SUPPLY_KERNEL_STR)  != std::string::npos) ||
	 (func.getName().str().find(COMPUTE_KERNEL_STR) != std::string::npos)) ||
	(func.getName().str().find(KERNEL_STR) != std::string::npos)
	) {
      return true;
    }
    return false;  
  }

  
  bool isa_dec_atomic(Instruction *I){
    if (isa<CallInst>(I)){
      CallInst *tmp = dyn_cast<CallInst>(&(*I));
      Function *fun = tmp->getCalledFunction();
      auto F_name = fun->getName().str();
      if (F_name.find("dec_atomic") != std::string::npos) {
	return true;
      }
    }
    return false;
  }

  bool check_indirect_load(Instruction *I, Instruction *LoopStart) {
    Value * val = I->getOperand(0);
    if (isa<Instruction>(val)){
      I = dyn_cast<Instruction>(&(*val));
    }
    while (true) {
      if(isa<LoadInst>(I)) {
	  return true;
      }
      else if (isa<GetElementPtrInst>(I)){
	Value * val = I->getOperand(1);
	if (isa<Instruction>(val)){
	  Instruction *tmp = dyn_cast<Instruction>(&(*val));
	  I=tmp;
	}
	else {
	  return false;
	}
      } else if (isa<SExtInst>(I)) {
	Value * val = I->getOperand(0);
	if (isa<Instruction>(val)){
	  Instruction *tmp = dyn_cast<Instruction>(&(*val));
	  I=tmp;
	} else {
	  return false;
	}
      } else {
	return false;
      }
    }
  }

  
  void findTheNMA(Instruction *I, Loop *L, Instruction *LoopEnd, int depth){
    if (isa<LoadInst>(*I) || isa_dec_atomic(I)) {
      if (check_indirect_load(I, LoopEnd)){
	if (NMADepth==0 && CandidateNMAs.size()==0){
	  NMADepth=depth;
	  CandidateNMAs.push_back(&(*I));
	  LoopEndInstrs.push_back(&(*LoopEnd));
	  Loops.push_back(&(*L));
	}
	else if (depth > NMADepth) {
	  NMADepth = depth;
	  while (CandidateNMAs.size()>0){
	    CandidateNMAs.pop_back();
	    LoopEndInstrs.pop_back();
	    Loops.pop_back();	  
	  }
	  CandidateNMAs.push_back(&(*I));
	  LoopEndInstrs.push_back(&(*LoopEnd));
	  Loops.push_back(&(*L));
	}
	else if (depth == NMADepth) {
	  CandidateNMAs.push_back(&(*I));
	  LoopEndInstrs.push_back(&(*LoopEnd));
	  Loops.push_back(&(*L));
	}
      }
    }
  }

  
  void findNMAsInLoop(Loop *L, unsigned nesting){
    unsigned numBlocks=0;
    Loop::block_iterator bb;
    int bbCounter =0;
    for (bb= L->block_begin(); bb!=L->block_end();++bb){
      //int depth = L->getLoopDepth();
      bbCounter+=1;
      //errs() << "[Block #" << bbCounter << " loop depth:"<< nesting << "]\n";
      for (BasicBlock::iterator I = (*bb)->begin(), IE=(*bb)->end(); I != IE; ++I){
	Instruction* iI = dyn_cast<Instruction>(&(*I));
	Instruction* loopEnd = dyn_cast<Instruction>(&(*IE));
	Instruction* loopStart = dyn_cast<Instruction>(&(*((*bb)->begin())));
	//errs() << "[Starting findNMA on " << *(iI) << "]\n";
	findTheNMA(iI, L, loopEnd, nesting);
      }
      numBlocks++;
    }
    //errs() << "[Loop level " << nesting<< " has " << numBlocks << " blocks]\n";
    vector<Loop*> subLoops = L->getSubLoops();
    Loop::iterator j, f;
    for (j = subLoops.begin(), f = subLoops.end(); j != f; ++j)
      findNMAsInLoop(*j, nesting + 1);
  }

 Function *getComputeExclusiveFunction(Instruction *I){
   if (isa<LoadInst>(*I)){
     LoadInst *LI = dyn_cast<LoadInst>(&(*I));
     Value *V = LI->getOperand(0);
     errs() << "[ Load Type: " << *(V->getType()) << " ]\n";
     if (V->getType()->isIntegerTy(32)){
       return compute_exclusive_load;
     }
     else if (V->getType()->isFloatTy()){
       return compute_exclusive_load_float;
     }
     
   }
   else if (isa<StoreInst>(*I)){
     StoreInst *SI = dyn_cast<StoreInst>(&(*I));
     Value *V = SI->getValueOperand();
     if (V->getType()->isIntegerTy(32)){
       return compute_exclusive_store;
     }
     else if (V->getType()->isIntegerTy(64)){
     }
     else if (V->getType()->isFloatTy()) {
       return compute_exclusive_store_float;
     }
     else if (V->getType()->isDoubleTy()) {
       return compute_exclusive_store_double;
     }
     else if (V->getType()->isVoidTy()) {
       return compute_exclusive_store_ptr;
     }
   }
 }

  
  bool isNMADependent(Instruction *I) {
    if (I->isIdenticalTo(CandidateNMAs[0])){
      return true;
    }
    
    if (DependentInstructions.find(I) == DependentInstructions.end()){
      DependentInstructions.insert(I);
      //errs() << "[Checking if Instruction " <<  *I <<  " is NMA dependent. Num Operands:";
      //errs() << I->getNumOperands()  <<  " ]\n";
      //vector<Value *> ops(I->op_begin(), I->op_end());
      for (int i=0; i< (I->getNumOperands()); i++){
	Value * val = I->getOperand(i);
	if (isa<Instruction>(val)){
	  Instruction * vI = dyn_cast<Instruction>(&(*val));
	  for (int i=0; i< CandidateNMAs.size(); i++){
	    if (vI->isIdenticalTo(CandidateNMAs[i])){
	      errs() << "[Instruction " <<  *I <<  " is NMA dependent!!]\n";
	      return true;
	    } else if (isNMADependent(vI)) {
	      return true;
	    }
	  }
	} 
      }
      //errs() << "[Parent (BB) Instruction " << *(I->getParent()) << "]\n";
      return false;
    }
    return false;    
  }

  
  void refactorInstruction(Instruction *I) {
    Instruction *Intr;
    //Value *Intr2;
    //errs() << "[Refactoring Instruction:" <<  *I <<  "]\n";
    if (isa<LoadInst>(*I)) {
      errs() << "[Is a Load Instruction:" <<  *I <<  "]\n";
      LoadInst * li = dyn_cast<LoadInst>(&(*I));
      IRBuilder<> Builder(li);
      Function *computeExclusiveLoad = getComputeExclusiveFunction(I);
      errs() << "[GOT HERE !!!!]\n";
      //errs() << "[ computeExclusiveLoad: " << *computeExclusiveLoad << " ]\n";
      Intr =  Builder.CreateCall(computeExclusiveLoad, {li->getOperand(0)});
      li->replaceAllUsesWith(Intr);
      errs() << "[Refactored Load Instruction]\n";
      to_erase.push_back(li);
    }
    else if (isa<CallInst>(*I)) {
      errs() << "[Is a Call Instruction:" <<  *I <<  "]\n";
      CallInst * ci = dyn_cast<CallInst>(&(*I));
      IRBuilder<> Builder(ci);      
      if (ci->getCalledFunction()->getName().str().find("dec_atomic_fetch_add") != std::string::npos) {
	vector<Value *> args(ci->arg_begin(), ci->arg_end());
	Intr =  Builder.CreateCall(compute_exclusive_fetch_add, args);
	ci->replaceAllUsesWith(Intr);
	errs() << "[Refactored dec_atomic_fetch_add Instruction]\n";
	to_erase.push_back(ci);
      }
      else if (ci->getCalledFunction()->getName().str().find("dec_atomic_fetch_min") != std::string::npos){
	Intr =  Builder.CreateCall(compute_exclusive_fetch_min, {ci->getOperand(0), ci->getOperand(1), ci->getOperand(2)});
	ci->replaceAllUsesWith(Intr);
	errs() << "[Refactored dec_atomic_fetch_min Instruction]\n";
	to_erase.push_back(ci);
      }
      else if(ci->getCalledFunction()->getName().str().find("dec_atomic_compare_exchange") != std::string::npos){
	vector<Value *> args(ci->arg_begin(), ci->arg_end());
	//Intr =  Builder.CreateCall(compute_exclusive_compare_exchange, {ci->getOperand(0), ci->getOperand(1), ci->getOperand(2), ci->getOperand(3)});
	Intr = Builder.CreateCall(compute_exclusive_compare_exchange, args);
	ci->replaceAllUsesWith(Intr);
	errs() << "[Refactored dec_atomic_compare_exchange]\n";
	to_erase.push_back(ci);
      }
    }	
  }

  void refactorBranchBlock(Instruction* I){
    BranchInst *bI = dyn_cast<BranchInst>(&(*I));
    BasicBlock* BB = bI->getSuccessor(0);
    for (BasicBlock::iterator iI = (*BB).begin(), IE=(*BB).end(); iI != IE; ++iI){
      errs() << "[BB: " <<  *iI << "]\n";
      Instruction* sI = dyn_cast<Instruction>(&(*iI));
      if (DependentInstructions.find(sI) == DependentInstructions.end()){
	DependentInstructions.insert(sI);
	refactorInstruction(sI);
	if (isa<BranchInst>(sI)){
	  BranchInst *bsI = dyn_cast<BranchInst>(&(*sI));
	  BasicBlock* b = bsI->getSuccessor(0);
	  if (Loops[0]->contains(&*b)) {
	    refactorBranchBlock(sI);
	  }
	  //int n = bsI->getNumSuccessors();
	  //for (int i=1; i<n; i++){
	  //  b = bsI->getSuccessor(i);
	  //  if (Loops[0]->contains(&*b)) {
	  //    refactorBranchBlock(sI);
	  //  }
	  //}
	}
      }
    } 
  }  
  void refactorNMADependentInstructions(Function &F){
    bool passedNMA = false;
    bool beforeEndOfLoop = true;
    for (inst_iterator iI = inst_begin(&F), iE = inst_end(&F); iI != iE; ++iI) {
      if (passedNMA && beforeEndOfLoop) {
	if (iI->isIdenticalTo(LoopEndInstrs[0])){
	  beforeEndOfLoop = false;
	}
	else {
	  Instruction *rI = dyn_cast<Instruction>(&(*iI));
	  if (isNMADependent(rI)) {
	    errs() << "[Refactoring NMA Dependent Instruction:" <<  *rI <<  "]\n";
	    DependentInstructions.clear();
	    refactorInstruction(rI);
	    CandidateNMAs.push_back(rI);
	    if (isa<BranchInst>(rI)){
	      BranchInst *bI = dyn_cast<BranchInst>(&(*rI));
	      BasicBlock* b = bI->getSuccessor(0);
	      if (Loops[0]->contains(&*b)) {
		refactorBranchBlock(rI);
	      }
	      //int n = bI->getNumSuccessors();
	      //for (int i=1; i<n; i++){
	      //b = bI->getSuccessor(i);
	      //if (Loops[0]->contains(&*b)) {
	      //  refactorBranchBlock(rI);
	      //}
	      //}
	    }
	  }
	}
      }
      if (!passedNMA) {
	//compare to NMA to see if we found it if yes passedNMA=true
	if (iI->isIdenticalTo(CandidateNMAs[0])){
	  passedNMA = true;
	}
      } 
    }
    
    for (int i = 0; i < to_erase.size(); i++) {
      to_erase[i]->eraseFromParent();
    }
    to_erase.clear();
    errs() << "[End of NMA Dependent Refactoring]\n";
  }

  struct FindNMAPass : public FunctionPass {
    static char ID;
    FindNMAPass() : FunctionPass(ID) {}
    int StartNMAPass =0;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesCFG();
      AU.addRequired<LoopInfoWrapperPass>();
    }
    
    virtual bool runOnFunction(Function &F) override {

      if (StartNMAPass==0) {
	errs() << "[Find NMA Pass Begin]\n";
	StartNMAPass =1;
      }

      if (isKernelFunction(F)) {
	errs() << "[Found Kernel]\n";
	populate_functions(F);
        errs() << "[" << F.getName().str() << "]\n";  
	LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
	int loopCounter = 0;
	for (LoopInfo::iterator li=LI.begin(), le=LI.end(); li!=le; ++li){
	  findNMAsInLoop(*li,0);
	}
	errs() << "[Number of Candidates: " << (CandidateNMAs.size()) << "]\n";
	for (int i=0;i<CandidateNMAs.size();i++){
	  errs() << "[" << *(CandidateNMAs[i]) << "]\n";
	}
	
	if (CandidateNMAs.size()==1) {
	  refactorNMADependentInstructions(F);
	}
      }
      for (int i=0; i< CandidateNMAs.size(); i++){
	CandidateNMAs.pop_back();
      }
      NMADepth = 0;
      errs() << "[Find NMA Pass End For Kernel";
      errs() << F.getName().str() << "]\n";
      return(false);
    }
    
  };
}

char FindNMAPass::ID = 0;
static RegisterPass<FindNMAPass> X("findnma", "Find NMA Pass", false, false);

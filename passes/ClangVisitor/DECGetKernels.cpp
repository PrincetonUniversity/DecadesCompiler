// Starting with example from: https://github.com/eliben/llvm-clang-samples/blob/master/src_clang/tooling_sample.cpp

#include "stdio.h"
#include <stdlib.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

int DECADES_thread_number = 1;
bool DECADES_decoupled = false;
bool DECADES_decoupled_explicit = false;


static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");

bool is_kernel_function(std::string s) {
  if (s.compare("_kernel_") == 0) {
    return true;
  }
  return false;
}

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
  MyASTVisitor(Rewriter &R) : TheRewriter(R) {}
  
  bool VisitStmt(Stmt *s) {

    if (isa<CallExpr>(s)) {

      CallExpr *ce = cast<CallExpr>(s);
      Expr *e = cast<Expr>(s);
      auto callee = ce->getCallee();
      std::string my_string;
      raw_string_ostream rso(my_string);
      callee->printPretty(rso, NULL, PrintingPolicy(LangOptions()));
      rso.flush();
      //std::cout << my_string << std::endl;

      if (is_kernel_function(my_string)) {
	std::stringstream SSBefore;
	std::string DECADES_thread_number_str = std::to_string(DECADES_thread_number);

	if (DECADES_decoupled) {
	  DECADES_thread_number *= 2;
	}
	SSBefore << "\n//DECADES OpenMP addition\n";
	SSBefore << "omp_set_dynamic(0);\n";
	SSBefore << "omp_set_num_threads(" << DECADES_thread_number << ");\n";
	SSBefore << "#pragma omp parallel\n{\n";
	SSBefore << "int DECADES_tid = omp_get_thread_num();\n";

	
	std::string compute_kernel_name = "_kernel_compute";
	std::string supply_kernel_name = "_kernel_supply";
	std::string args = "";
	std::string supply_args = "";
	for (int i = 0; i < ce->getNumArgs() - 2; i++) {
	  my_string = "";
	  ce->getArg(i)->printPretty(rso, NULL, PrintingPolicy(LangOptions()));
	  rso.flush();
	  args.append(my_string);
	  supply_args.append(my_string);	  
	  args.append(",");
	  supply_args.append(",");
	}
	args.append("DECADES_tid,");
	supply_args.append("(DECADES_tid - (omp_get_num_threads()/2)),");
	args.append(DECADES_thread_number_str);
	supply_args.append(DECADES_thread_number_str);
	std::string DECADES_COMPUTE_CALL = compute_kernel_name + "(" + args + ");";
	std::string DECADES_SUPPLY_CALL = supply_kernel_name + "(" + supply_args + ");";
	if (DECADES_decoupled) {
	  SSBefore << "if (DECADES_tid < " << DECADES_thread_number_str << ") {\n";
	  SSBefore << DECADES_COMPUTE_CALL + "\n}\n";
	  SSBefore << "else {\n";
	  SSBefore << DECADES_SUPPLY_CALL + "\n}\n";
	}
	else {
	  SSBefore << DECADES_COMPUTE_CALL << "\n";
	}
	SSBefore << "//";
	
	SourceLocation ST = e->getSourceRange().getBegin();
	TheRewriter.InsertText(ST, SSBefore.str(), true, true);
	std::stringstream SSAfter;
	SSAfter << "\n}\n//Finished DECADES OpenMP addition";
	ST = e->getLocEnd().getLocWithOffset(2);
	TheRewriter.InsertText(ST, SSAfter.str(), true, true);
      }

    }
    
    return true;
  }
  
  bool VisitFunctionDecl(FunctionDecl *f) {
    // Only function definitions (with bodies), not declarations.
    if (f->hasBody()) {
      DeclarationName DeclName = f->getNameInfo().getName();
      std::string FuncName = DeclName.getAsString();
      if (is_kernel_function(FuncName)) {
	std::string compute_kernel_name = "__attribute__((noinline)) void _kernel_compute";
	std::string supply_kernel_name = "__attribute__((noinline)) void _kernel_supply";	
	Stmt *FuncBody = f->getBody();
	std::string my_string;
	raw_string_ostream rso(my_string);

	std::string args = "";
	for (int i = 0; i < f->getNumParams(); i++) {
	  my_string = "";
	  f->getParamDecl(i)->print(rso);
	  rso.flush();
	  args.append(my_string);
	  if (i != f->getNumParams() -1) {
	    args.append(", ");
	  }
	}
	my_string = "";

	FuncBody->printPretty(rso, NULL, PrintingPolicy(LangOptions()));
	rso.flush();


	// Type name as string
	// Add comment before
	std::stringstream SSBefore;
	SSBefore << compute_kernel_name << "(" << args << ")\n";
	SSBefore << my_string << "\n\n";
	SSBefore << supply_kernel_name << "(" << args << ")\n";
	SSBefore << my_string << "\n\n";

	SourceLocation ST = f->getSourceRange().getBegin();
	TheRewriter.InsertText(ST, SSBefore.str(), true, true);
	
	// And after
      }
    }
    
    return true;
  }
  
private:
  Rewriter &TheRewriter;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(Rewriter &R) : Visitor(R) {}
  
  // Override the method that gets called for each parsed top-level
  // declaration.
  bool HandleTopLevelDecl(DeclGroupRef DR) override {
    for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
      // Traverse the declaration using our AST visitor.
      Visitor.TraverseDecl(*b);
      (*b)->dump();
    }
    return true;
  }
  
private:
  MyASTVisitor Visitor;
};


// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction() {}
  void EndSourceFileAction() override {
    SourceManager &SM = TheRewriter.getSourceMgr();
    llvm::errs() << "** EndSourceFileAction for: "
                 << SM.getFileEntryForID(SM.getMainFileID())->getName() << "\n";
    
    // Now emit the rewritten buffer.
    TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
  }
  
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    llvm::errs() << "** Creating AST consumer for: " << file << "\n";
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return llvm::make_unique<MyASTConsumer>(TheRewriter);
  }
  
private:
  Rewriter TheRewriter;
};

int main(int argc, const char **argv) {

  auto env_dec_thread_num = getenv("DECADES_THREAD_NUM");
  auto env_dec_decoupled = getenv("DECADES_DECOUPLED");
  auto env_dec_decoupled_explicit = getenv("DECADES_DECOUPLED_EXPLICIT");

  if (env_dec_thread_num) {
    DECADES_thread_number = atoi(env_dec_thread_num);
  }
  if (env_dec_decoupled){
    if (atoi(env_dec_decoupled) == 1) {
      DECADES_decoupled = true;
    }
  }
  if (env_dec_decoupled_explicit) {
    if (atoi(env_dec_decoupled_explicit) == 1) {
      DECADES_decoupled_explicit = true;
    }
  }

  CommonOptionsParser op(argc, argv, ToolingSampleCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
  printf("hello world\n");
  return 0;
}

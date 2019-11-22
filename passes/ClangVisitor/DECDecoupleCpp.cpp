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

  CommonOptionsParser op(argc, argv, ToolingSampleCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}

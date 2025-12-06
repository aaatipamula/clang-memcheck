#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <unordered_map>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

enum PointerState {
  UNKNOWN,
  FREE,
  OWNED
};

typedef std::unordered_map<int64_t, PointerState> MemStateMap;

// Your AST Visitor - this is where you'll implement the analysis
class MemoryAnalysisVisitor : public RecursiveASTVisitor<MemoryAnalysisVisitor> {
private:
  ASTContext *Context;
  MemStateMap stateMap = MemStateMap();
  int64_t currVar = 0;

  void printError(Stmt *node, const std::string message) {
    if (node) {
      SourceManager &sm = Context->getSourceManager();
      SourceLocation loc = node->getBeginLoc();
      
      // Print in standard compiler error format
      loc.print(llvm::errs(), sm);
      llvm::errs() << ": ";
    }

    llvm::errs() << "error: " << message << "\n";
  }

  void printInfo(const std::string message) {
    llvm::outs() << "info: " << message << "\n";
  }

  VarDecl *unwrapExpr(Expr *expression) {
    if (!expression) return nullptr;

    // Unwrap the argument to get to a var decl
    Expr *Exp = expression->IgnoreParenImpCasts();
    DeclRefExpr *Decl = dyn_cast<DeclRefExpr>(Exp);

    // Free was not given a reference to a declaration
    if (!Decl) {
      // printError(Exp, "Unexpected expr (not DeclRefExpr)");
      return nullptr;
    }

    VarDecl *Var = dyn_cast<VarDecl>(Decl->getDecl());

    // Free was not given a reference to a variable
    if (!Var) {
      // printError(Decl, "Unexpected declaration (not VarDecl)");
      return nullptr;
    }

    return Var;
  }

  bool checkDeref(UnaryOperator *UnOp) {
    // Ignore non dereferences
    if (UnOp->getOpcode() != clang::UO_Deref) {
      return true;
    }

    // Unwrap the expression to the unary operator to expose a vardecl
    VarDecl *Var = unwrapExpr(UnOp->getSubExpr());

    if (!Var) {
      return true;
    }

    int64_t varId = Var->getID();
    PointerState currState = UNKNOWN;

    // Derefed var does not have heap memory attached to it
    try {
      currState = stateMap.at(varId);
    } catch (std::out_of_range err) {
      return false;
    }

    switch (currState) {
      case FREE:
        printError(UnOp, "Tried to deref freed memory");
        return false;

      case UNKNOWN:
        printError(UnOp, "Current state of memory is unknown here");
        return false;

      case OWNED:
        break;

      default:
        printInfo("Var does not have allocated memory attached");
    }

    return true;
  }

  bool checkIndex(ArraySubscriptExpr *ArrayIndex) {
    VarDecl *Var = unwrapExpr(ArrayIndex->getBase());

    if (!Var) {
      return true;
    }

    int64_t varId = Var->getID();
    PointerState currState = UNKNOWN;

    // Derefed var does not have heap memory attached to it
    try {
      currState = stateMap.at(varId);
    } catch (std::out_of_range err) {
      return false;
    }

    switch (currState) {
      case FREE:
        printError(ArrayIndex, "Tried to index into freed memory");
        return false;

      case UNKNOWN:
        printError(ArrayIndex, "Current state of memory is unknown here");
        return false;

      case OWNED:
        break;

      default:
        printInfo("Var does not have allocated memory attached");
    }

    return true;
  }

public:
  explicit MemoryAnalysisVisitor(ASTContext *Context) : Context(Context) {}

  bool checkMemState() {
    for (const auto entry : stateMap) {
      if (entry.second == OWNED) {
        printError(nullptr, "There is potentially unfreed memory");
        return false;
      } else if (entry.second == UNKNOWN) {
        printError(nullptr, "Memory state is unknown");
        return false;
      }
    }

    return true;
  }

  // Set the current variable
  bool VisitVarDecl(VarDecl *Var) {
    if (Var->hasInit()) {
      currVar = Var->getID();
      // printInfo("Var ID: " + std::to_string(currVar));
    }

    return true;
  }

  // Visit function calls to check for malloc, free, etc.
  bool VisitCallExpr(CallExpr *Call) {
    const FunctionDecl *callee = Call->getDirectCallee();
    
    // Ignore function pointers for now
    if (!callee) {
      return true;
    }

    // Get function name
    std::string functionName = callee->getNameAsString();

    if (functionName == "malloc" || functionName == "calloc") {
      if (currVar == 0) {
        printError(Call, "Allocated memory is not assigned to a variable!");
        return false;
      }

      // printInfo("Allocating Var: " + std::to_string(currVar));
      stateMap.insert({currVar, OWNED});

    } else if (functionName == "realloc") {

      if (currVar == 0) {
        printError(Call, "Reallocated memory is not assigned to variable!");
        return false;
      }

      // printInfo("Reallocating Var: " + std::to_string(currVar));

      Expr *Arg = Call->getArg(0);

      // Unwrap the argument to get to a var decl
      VarDecl *Var = unwrapExpr(Arg);

      if (!Var) {
        printError(Call, "realloc was not called with a variable");
        return false;
      }

      if (Var->getID() == currVar) {
        printError(Call, "Cannot reallocate to same variable.");
        return false;
      }

      PointerState currState = UNKNOWN;

      try {
        currState = stateMap.at(currVar);
      } catch (std::out_of_range) {
        // printInfo("Variable was not previously allocated");
        currState = UNKNOWN;
      }

      switch (currState) {
        case FREE:
          stateMap.at(currVar) = OWNED;
          break;
        case UNKNOWN:
          stateMap.insert({currVar, OWNED});
          break;
        case OWNED:
          printError(Call, "Cannot reallocate to variable pointing into heap");
          return false;
      }
      
    } else if (functionName == "free") {
      // There should only be one argument to free
      Expr *Arg = Call->getArg(0);

      // Unwrap the argument to get to a var decl
      VarDecl *Var = unwrapExpr(Arg);

      if (!Var) {
        printError(Call, "free was not called with a variable");
        return false;
      }

      // Get the identifier associated with the variable
      int64_t varId = Var->getID();
      PointerState currState = UNKNOWN;

      try {
        currState = stateMap.at(varId);
      } catch (std::out_of_range) {
        // return false;
      }

      // printInfo("Attempting to free: " + std::to_string(varId));

      switch (currState) {
        case FREE:
          printError(Call, "Double free of memory");
          return false;

        case UNKNOWN:
          printError(Call, "Current state of memory is unknown here");
          return false;

        case OWNED:
          stateMap.at(varId) = FREE;
          break;
      }
    }

    return true;
  }

  bool VisitBinaryOperator(BinaryOperator *BinOp) {
    // Ignore non assignment operators
    if (BinOp->getOpcode() != clang::BO_Assign) {
      return true;
    }

    Expr * lhs = BinOp->getLHS();

    // Check the unary for a deref of heap memory
    UnaryOperator *UnOp = dyn_cast<UnaryOperator>(lhs->IgnoreParenImpCasts());
    if (UnOp) {
      return checkDeref(UnOp);
    }

    // Check array indexes for index into heap memory
    ArraySubscriptExpr *ArrayIndex = dyn_cast<ArraySubscriptExpr>(lhs->IgnoreParenImpCasts());
    if (ArrayIndex) {
      return checkIndex(ArrayIndex);
    }

    VarDecl *LHSVar = unwrapExpr(lhs);

    // Ignore if not variable
    if (!LHSVar) {
      return true;
    }

    Expr * rhs = BinOp->getRHS();
    VarDecl *RHSVar = unwrapExpr(rhs);

    // One var being assigned to other have same type
    if (RHSVar && RHSVar->getType() == LHSVar->getType()) {
      // TODO: Create mapping of var ids for aliases
      int64_t rhsVarId = RHSVar->getID();
      PointerState rhsState = UNKNOWN;

      try {
        rhsState = stateMap.at(rhsVarId);
      } catch (std::out_of_range err) {
        // rhs isn't a variable which points to heap memory (should be fine)
      }

      switch (rhsState) {
        case OWNED:
          printError(BinOp, "Tried to alias pointer into heap memory");
          return false;
        case UNKNOWN:
        case FREE:
          break;
      }
    }

    int64_t varId = LHSVar->getID();
    PointerState currState = UNKNOWN;

    // Check variable accesses and ignore any that are not assigned heap memory
    try {
      currState = stateMap.at(varId);
    } catch (std::out_of_range err) {
      return true;
    }

    switch (currState) {
      case OWNED:
        printError(BinOp, "Variable being assigned to has not freed its memory");
        return false;
      case UNKNOWN:
        printError(BinOp, "Current state of memory is unknown here");
        return false;
      case FREE:
        break;
    }

    return true;
  }

  bool VisitReturnStmt(ReturnStmt *Stmt) {
    Expr * RetExpr = Stmt->getRetValue();

    if (!RetExpr) {
      return true;
    }

    VarDecl *Var = unwrapExpr(RetExpr);

    if (!Var) {
      return true;
    }

    int64_t varId = Var->getID();
    PointerState currState = UNKNOWN;

    // Check variable accesses and ignore any that are not assigned heap memory
    try {
      currState = stateMap.at(varId);
    } catch (std::out_of_range err) {
      return true;
    }

    switch (currState) {
      case OWNED:
        printError(Stmt, "Attempted to return pointer to heap memory");
        break;
      case FREE:
        /**
         * NOTE: Even freed memory is not allowed to be returned 
         *       (we don't know if it is NULL)
        */
        printError(Stmt, "Attempted to return freed pointer to heap memory");
        break;
      case UNKNOWN:
        printError(Stmt, "Current state of memory in return is unknown here");
        return false;
    }

    return true;
  }
};

// AST Consumer - creates and runs the visitor
class MemoryAnalysisConsumer : public ASTConsumer {
private:
  MemoryAnalysisVisitor Visitor;

public:
  explicit MemoryAnalysisConsumer(ASTContext *Context) : Visitor(Context) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    bool traverseSuccess = Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    bool memOkay = Visitor.checkMemState(); // Checks the state of memory after traversal (memory leaks)

    if (traverseSuccess && memOkay) {
      llvm::outs() << "Memory okay!\n";
    }  
  }
};

// Frontend Action - creates the consumer
class MemoryAnalysisAction : public ASTFrontendAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    return std::make_unique<MemoryAnalysisConsumer>(&CI.getASTContext());
  }
};

// Command line options
static cl::OptionCategory MemoryAnalyzerCategory("memory-analyzer options");

int main(int argc, const char **argv) {
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, MemoryAnalyzerCategory);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser &OptionsParser = ExpectedParser.get();
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  return Tool.run(newFrontendActionFactory<MemoryAnalysisAction>().get());
}


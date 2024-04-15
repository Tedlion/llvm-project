/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#include "DeclScanner.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"

namespace clang::class_wrapper {
using namespace clang::ast_matchers;

constexpr const char TypedefDeclStr[] = "typedefDecl";
auto TypedefDeclMatcher = traverse(TK_IgnoreUnlessSpelledInSource,
                                   typedefDecl().bind(TypedefDeclStr));
constexpr const char RecordDeclStr[] = "recordDecl";
auto RecordDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, recordDecl().bind(RecordDeclStr));
constexpr const char EnumDeclStr[] = "enumDecl";
auto EnumDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, enumDecl().bind(EnumDeclStr));
constexpr const char VarDeclStr[] = "varDecl";
auto VarDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, varDecl().bind(VarDeclStr));
constexpr const char FunctionDeclStr[] = "functionDecl";
auto FunctionDeclMatcher = traverse(TK_IgnoreUnlessSpelledInSource,
                                    functionDecl().bind(FunctionDeclStr));
constexpr const char DeclStmtStr[] = "declStmt";
auto DeclStmtMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, declStmt().bind(DeclStmtStr));
constexpr const char DeclRefExprStr[] = "declRefExpr";
auto DeclRefExprMatcher = traverse(TK_IgnoreUnlessSpelledInSource,
                                   declRefExpr().bind(DeclRefExprStr));
template<typename NodeType>
concept PrettyDumpNode = requires(const NodeType &Node, const ASTContext& Context) {
  Node.dumpPretty(Context);
};

template <typename MatcherHandler, typename NodeType, auto bindName>
class ScannerMatcherHandler : public MatchFinder::MatchCallback {
public:
  ScannerMatcherHandler(ClassWrapperContext &Context) : Context(Context) {}

  virtual void run(const MatchFinder::MatchResult &Result) override {
    const auto *Node = Result.Nodes.getNodeAs<NodeType>(bindName);
    if (!Node) {
      return;
    }
    StringRef FileName = Result.SourceManager->getFilename(Node->getBeginLoc());
    if (!Context.needToWrap(FileName)) {
      return;
    }

    debugDump(Result, *Node);
    auto *Handler = static_cast<MatcherHandler *>(this);
    Handler->run(Result, *Node);
  }

  // For temp usage
  void debugDump(const MatchFinder::MatchResult &Result, const NodeType &Node) {
    Node.dumpColor();
    if constexpr (PrettyDumpNode<NodeType>) {
      Node.dumpPretty(*Result.Context);
    }
  }

  ClassWrapperContext &Context;
};

class TypedefDeclHandler
    : public ScannerMatcherHandler<TypedefDeclHandler, TypedefDecl,
                                   TypedefDeclStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const TypedefDecl &TD) {
    // TODO
  }
};

class RecordDeclHandler
    : public ScannerMatcherHandler<RecordDeclHandler, RecordDecl,
                                   RecordDeclStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const RecordDecl &RD) {
    // TODO
  }
};

class EnumDeclHandler
    : public ScannerMatcherHandler<EnumDeclHandler, EnumDecl, EnumDeclStr> {
  using ScannerMatcherHandler::ScannerMatcherHandler;

public:
  void run(const MatchFinder::MatchResult &Result, const EnumDecl &ED) {
    // TODO
  }
};

class VarDeclHandler
    : public ScannerMatcherHandler<VarDeclHandler, VarDecl, VarDeclStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const VarDecl &VD) {
    // TODO
  }
};

class FunctionDeclHandler
    : public ScannerMatcherHandler<FunctionDeclHandler, FunctionDecl,
                                   FunctionDeclStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const FunctionDecl &FD) {
    // TODO
  }
};

class DeclStmtHandler
    : public ScannerMatcherHandler<DeclStmtHandler, DeclStmt, DeclStmtStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const DeclStmt &DS) {
    // TODO
  }
};

class DeclRefHandler : public ScannerMatcherHandler<DeclRefHandler, DeclRefExpr,
                                                    DeclRefExprStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const DeclRefExpr &DRE) {
    // TODO
  }
};

void runDeclScanner(const CompilationDatabase &Compilations,
                    const llvm::ArrayRef<std::string> SourcePaths,
                    ClassWrapperContext &Context) {
  ClangTool Tool(Compilations, SourcePaths);
  MatchFinder Finder;
  TypedefDeclHandler TypedefDeclHandler(Context);
  RecordDeclHandler RecordDeclHandler(Context);
  EnumDeclHandler EnumDeclHandler(Context);
  FunctionDeclHandler FunctionDeclHandler(Context);
  VarDeclHandler VarDeclHandler(Context);
  DeclStmtHandler DeclStmtHandler(Context);
  DeclRefHandler DeclRefHandler(Context);

  Finder.addMatcher(TypedefDeclMatcher, &TypedefDeclHandler);
  Finder.addMatcher(RecordDeclMatcher, &RecordDeclHandler);
  Finder.addMatcher(EnumDeclMatcher, &EnumDeclHandler);
  Finder.addMatcher(FunctionDeclMatcher, &FunctionDeclHandler);
  Finder.addMatcher(VarDeclMatcher, &VarDeclHandler);
  Finder.addMatcher(DeclStmtMatcher, &DeclStmtHandler);
  Finder.addMatcher(DeclRefExprMatcher, &DeclRefHandler);
  Tool.run(newFrontendActionFactory(&Finder).get());

  // TODO:
}

} // namespace clang::class_wrapper

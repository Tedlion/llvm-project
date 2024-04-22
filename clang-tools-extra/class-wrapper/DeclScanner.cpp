/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#include "DeclScanner.h"
#include "clang/AST/ODRHash.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Transformer/SourceCode.h"

namespace clang::class_wrapper {
using namespace clang::ast_matchers;

// SourceRange getFullRange(const ASTContext &Context, const SourceRange &Range)
// {
//   SourceLocation AfterEnd = clang::Lexer::getLocForEndOfToken(Range.getEnd(),
//   0, Context.getSourceManager(), Context.getLangOpts()); return
//   {Range.getBegin(), AfterEnd};
// }

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

namespace {
DenseMap<const Type *, unsigned> TypeHashMap;

unsigned getRecordDeclHash(const RecordDecl &RD) {
  ODRHash Hash;
  Hash.AddRecordDecl(&RD);
  for (const auto *Attr : RD.attrs()) {
    Hash.AddIdentifierInfo(Attr->getAttrName());
  }
  for (const auto *Field : RD.fields()) {
    QualType FieldType = Field->getType();
    if (FieldType->isRecordType()) {
      const Type *TypePtr = Field->getType().getTypePtr();
      assert(TypeHashMap.contains(TypePtr));
      Hash.AddStructuralValue(
          clang::APValue(llvm::APSInt(TypeHashMap[TypePtr])));
    }
  }

  unsigned HashValue = Hash.CalculateHash();
  const Type *SelfType = RD.getTypeForDecl();
  assert(SelfType);
  assert(!TypeHashMap.contains(SelfType));
  TypeHashMap[SelfType] = HashValue;
  return HashValue;
}

class ScannerSourceFileCallback : public SourceFileCallbacks {
  void handleEndSource() override { TypeHashMap.clear(); }

public:
  static ScannerSourceFileCallback *get() {
    static ScannerSourceFileCallback Instance;
    return &Instance;
  }
};

} // namespace

template <typename NodeType>
concept PrettyDumpNode =
    requires(const NodeType &Node, const ASTContext &Context) {
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

    //    debugDump(Result, *Node);
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
    debugDump(Result, TD);

    CharSourceRange FullRange = getAssociatedRange(TD, *Result.Context);
    llvm::errs() << getText(FullRange, *Result.Context) << "\n";

    //    clang::QualType AliasedType = TD.getUnderlyingType();
    //    ODRHash Hash;
    //    Hash.AddQualType(AliasedType);
    //    Hash.AddType(AliasedType.getTypePtr());
    //    auto HashValue = Hash.CalculateHash();
    //    llvm::errs() << std::format("{}:0x{:x}\n", TD.getName().data(),
    //    HashValue);

    //    Context.recordSymbol(TD.getName(), FullRange, TD.getKind(),
    //                         TD, 0, std::nullopt, false, false);
  }
};

class RecordDeclHandler
    : public ScannerMatcherHandler<RecordDeclHandler, RecordDecl,
                                   RecordDeclStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const RecordDecl &RD) {
    if (RD.getName().empty()) {
      return;
    }
    debugDump(Result, RD);

    CharSourceRange FullRange = getAssociatedRange(RD, *Result.Context);

    unsigned Hash = getRecordDeclHash(RD);

    llvm::errs() << getText(FullRange, *Result.Context)
                 << std::format("0x{:x}", Hash) << "\n";

    Context.recordSymbol(RD.getName(), FullRange, RD.getKind(),
                         StorageClass::SC_Extern, Hash, std::nullopt, false,
                         false);
  }
};

class EnumDeclHandler
    : public ScannerMatcherHandler<EnumDeclHandler, EnumDecl, EnumDeclStr> {
  using ScannerMatcherHandler::ScannerMatcherHandler;

public:
  void run(const MatchFinder::MatchResult &Result, const EnumDecl &ED) {
    debugDump(Result, ED);
    // TODO
  }
};

class VarDeclHandler
    : public ScannerMatcherHandler<VarDeclHandler, VarDecl, VarDeclStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const VarDecl &VD) {
    if (VD.hasLocalStorage()) {
      return;
    }

    debugDump(Result, VD);
    auto RangeStr = VD.getSourceRange().printToString(*Result.SourceManager);
    // TODO
  }
};

class FunctionDeclHandler
    : public ScannerMatcherHandler<FunctionDeclHandler, FunctionDecl,
                                   FunctionDeclStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const FunctionDecl &FD) {
    unsigned ODRHash = getDeclHash(FD);
    //    llvm::errs() << std::format("{}:0x{:x}\n", FD.getType().getAsString(),
    //    ODRHash);
    std::optional<unsigned> ImplHash = std::nullopt;
    if (FD.hasBody()) {
      Stmt *Body = FD.getBody();
      class ODRHash Hash;
      Hash.AddStmt(Body);
      ImplHash = Hash.CalculateHash();
      //        llvm::errs() << std::format("BodyHash :0x{:x}\n",
      //        ImplHash.value());
    }
    debugDump(Result, FD);
    CharSourceRange FullRange = getAssociatedRange(FD, *Result.Context);
    llvm::errs() << getText(FullRange, *Result.Context) << "\n";
    Context.recordSymbol(FD.getName(), FullRange, FD.getKind(),
                         FD.getStorageClass(), ODRHash, ImplHash,
                         FD.isInlineSpecified(), false);
  }

  static unsigned getDeclHash(const FunctionDecl &FD) {
    class ODRHash Hash;
    QualType DeclType = FD.getType(); // return type & param types
    Hash.AddQualType(DeclType);
    Hash.AddBoolean(FD.isStatic());
    return Hash.CalculateHash();
  }
};

class DeclStmtHandler
    : public ScannerMatcherHandler<DeclStmtHandler, DeclStmt, DeclStmtStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const DeclStmt &DS) {
    debugDump(Result, DS);
    // TODO
  }
};

class DeclRefHandler : public ScannerMatcherHandler<DeclRefHandler, DeclRefExpr,
                                                    DeclRefExprStr> {
public:
  using ScannerMatcherHandler::ScannerMatcherHandler;

  void run(const MatchFinder::MatchResult &Result, const DeclRefExpr &DRE) {
    debugDump(Result, DRE);
    // TODO
  }
};

void runDeclScanner(const CompilationDatabase &Compilations,
                    const llvm::ArrayRef<std::string> SourcePaths,
                    ClassWrapperContext &Context) {
  ClangTool Tool(Compilations, SourcePaths,
                 std::make_shared<PCHContainerOperations>(),
                 Context.getBaseFS(), Context.getFiles());
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

  Tool.run(newFrontendActionFactory(&Finder, ScannerSourceFileCallback::get())
               .get());

  // TODO:
}

} // namespace clang::class_wrapper

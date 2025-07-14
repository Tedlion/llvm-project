/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#include "DeclScanner.h"
#include "ExtendedODRHash.h"
#include "Support.h"

#include "clang/AST/ODRHash.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Transformer/SourceCode.h"

namespace clang::class_wrapper {

namespace {

//unsigned getRecordDeclHash(const RecordDecl &RD) {
//  ODRHash Hash;
//  Hash.AddRecordDecl(&RD);
//  for (const auto *Attr : RD.attrs()) {
//    Hash.AddIdentifierInfo(Attr->getAttrName());
//  }
//  for (const auto *Field : RD.fields()) {
//    QualType FieldType = Field->getType();
//    if (FieldType->isRecordType()) {
//      const Type *TypePtr = FieldType.getCanonicalType().getTypePtr();
//      TypePtr->dump();
//      assert(TypeHashMap.contains(TypePtr));
//      Hash.AddStructuralValue(
//          clang::APValue(llvm::APSInt(TypeHashMap[TypePtr])));
//    }
//  }
//
//  unsigned HashValue = Hash.CalculateHash();
//  const Type *SelfType = RD.getTypeForDecl();
//  assert(SelfType);
//  assert(!TypeHashMap.contains(SelfType));
//  SelfType->dump();
//  TypeHashMap[SelfType] = HashValue;
//  llvm::errs() << std::format("{} \n", (void *)SelfType);
//  return HashValue;
//}



} // namespace

template <typename NodeType>
concept PrettyDumpNode =
    requires(const NodeType &Node, const ASTContext &Context) {
      Node.dumpPretty(Context);
    };

template <typename MatcherHandler, typename NodeType, auto bindName>
class ScannerMatcherHandler : public MatchFinder::MatchCallback {
public:
  ScannerMatcherHandler(const NeedToWrapFunc &NeedToWrap,
                        const RecordSymbolFunc &RecordSymbol,
                        ExtendedODRHash::ODRHashCache &Cache)
      : NeedToWrap(NeedToWrap), RecordSymbol(RecordSymbol),
        TypeHashCache(Cache) {}

  virtual void run(const MatchFinder::MatchResult &Result) override {
    const auto *Node = Result.Nodes.getNodeAs<NodeType>(bindName);
    if (!Node) {
      return;
    }

    StringRef FileName = Result.SourceManager->getFilename(Node->getBeginLoc());
    if (!NeedToWrap(FileName)) {
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

  const NeedToWrapFunc &NeedToWrap;
  const RecordSymbolFunc &RecordSymbol;
  ExtendedODRHash::ODRHashCache &TypeHashCache;
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

    // TODO: check if the underlying is a RecordDecl or EnumDecl
    //  Deletion the old code redundantly is fine, but be care of the insertion action

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

    if (!RD.isCompleteDefinition()) {
      return;
    }

    auto ParentNode = Result.Context->getParents(RD);
    if (ParentNode.size() != 1 ||
        !ParentNode[0].getNodeKind().isSame(
            ASTNodeKind::getFromNodeKind<clang::TranslationUnitDecl>())) {
      return;
      }

    auto HashValue =
        ExtendedODRHash::calculateRecordDeclHash(&RD, TypeHashCache);

    Replacement Replace(*Result.SourceManager, FullRange, StringRef());

    RecordSymbol(SymbolRecordEntry{
        RD.getName().str(), Replace.getFilePath().str(),
        tooling::Range(Replace.getOffset(), Replace.getLength()), RD.getKind(),
        StorageClass::SC_Extern, ExtendedODRHash::HashValueInvalid, HashValue,
        false, false});
    //    unsigned Hash = getRecordDeclHash(RD);
//    llvm::errs() << std::format(
//        "{} 0x{:x}\n", getText(FullRange, *Result.Context), Hash);

//    RecordSymbol(RD.getName(), FullRange, RD.getKind(),
//                         StorageClass::SC_Extern, Hash, std::nullopt, false,
//                         false);
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
    // FIXME:
//    unsigned ODRHash = getDeclHash(FD);
//    llvm::errs() << std::format("{}:0x{:x}\n", FD.getType().getAsString(),
//                                ODRHash);
//    std::optional<unsigned> ImplHash = std::nullopt;
//    if (FD.hasBody()) {
//      Stmt *Body = FD.getBody();
//      class ODRHash Hash;
//
//      Hash.AddStmt(Body);
//      ImplHash = Hash.CalculateHash();
//      //        llvm::errs() << std::format("BodyHash :0x{:x}\n",
//      //        ImplHash.value());
//    }
//    debugDump(Result, FD);
//    CharSourceRange FullRange = getAssociatedRange(FD, *Result.Context);
//    llvm::errs() << getText(FullRange, *Result.Context) << "\n";
//    RecordSymbol(FD.getName(), FullRange, FD.getKind(),
//                         FD.getStorageClass(), ODRHash, ImplHash,
//                         FD.isInlineSpecified(), false);
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

std::unique_ptr<MatchFinder> newDeclScannerMatchFinderFactory(
    const NeedToWrapFunc &NeedToWrap, const RecordSymbolFunc &RecordSymbol,
    const std::shared_ptr<ExtendedODRHash::ODRHashCache> & TypeHashCache) {
  class DeclScannerMatchFinder : public MatchFinder {
  private:
    const NeedToWrapFunc MNeedToWrap;
    const RecordSymbolFunc MRecordSymbol;
    std::shared_ptr<ExtendedODRHash::ODRHashCache> MTypeHashCache;
    TypedefDeclHandler TypedefDeclHandler;
    RecordDeclHandler RecordDeclHandler;
    EnumDeclHandler EnumDeclHandler;
    FunctionDeclHandler FunctionDeclHandler;
    VarDeclHandler VarDeclHandler;
    DeclStmtHandler DeclStmtHandler;
    DeclRefHandler DeclRefHandler;

  public:
    DeclScannerMatchFinder(
        const NeedToWrapFunc &NeedToWrap, const RecordSymbolFunc &RecordSymbol,
        const std::shared_ptr<ExtendedODRHash::ODRHashCache> &TypeHashCache)
        : MNeedToWrap(NeedToWrap), MRecordSymbol(RecordSymbol),
          MTypeHashCache(TypeHashCache),
          TypedefDeclHandler(MNeedToWrap, MRecordSymbol, *MTypeHashCache),
          RecordDeclHandler(MNeedToWrap, MRecordSymbol, *MTypeHashCache),
          EnumDeclHandler(MNeedToWrap, MRecordSymbol, *MTypeHashCache),
          FunctionDeclHandler(MNeedToWrap, MRecordSymbol, *MTypeHashCache),
          VarDeclHandler(MNeedToWrap, MRecordSymbol, *MTypeHashCache),
          DeclStmtHandler(MNeedToWrap, MRecordSymbol, *MTypeHashCache),
          DeclRefHandler(MNeedToWrap, MRecordSymbol, *MTypeHashCache) {
      addMatcher(TypedefDeclMatcher, &TypedefDeclHandler);
      addMatcher(RecordDeclMatcher, &RecordDeclHandler);
      addMatcher(EnumDeclMatcher, &EnumDeclHandler);
      addMatcher(FunctionDeclMatcher, &FunctionDeclHandler);
      addMatcher(VarDeclMatcher, &VarDeclHandler);
      addMatcher(DeclStmtMatcher, &DeclStmtHandler);
      addMatcher(DeclRefExprMatcher, &DeclRefHandler);
    }
  };

  return std::make_unique<DeclScannerMatchFinder>(NeedToWrap, RecordSymbol,
                                                  TypeHashCache);
}

void runDeclScanner(const CompilationDatabase &Compilations,
                    const llvm::ArrayRef<std::string> SourcePaths,
                    ClassWrapperContext &Context) {
  ClangTool Tool(Compilations, SourcePaths,
                 std::make_shared<PCHContainerOperations>(),
                 Context.getBaseFS(), Context.getFiles());

  NeedToWrapFunc NeedToWrap = [&Context](const StringRef &FileName) {
    return Context.needToWrap(FileName);
  };

  auto RecordSymbol =
      std::bind_front(&ClassWrapperContext::recordSymbol, Context);

  auto HashCache = std::make_shared<ExtendedODRHash::ODRHashCache>();

  class ScannerSourceFileCallback : public SourceFileCallbacks {
  private:
    std::shared_ptr<ExtendedODRHash::ODRHashCache> TypeHashCache;

  public:
    ScannerSourceFileCallback(std::shared_ptr<ExtendedODRHash::ODRHashCache>& Cache)
        : TypeHashCache(Cache) {}

    void handleEndSource() override { TypeHashCache->clear(); }
  };

  auto Finder =
      newDeclScannerMatchFinderFactory(NeedToWrap, RecordSymbol, HashCache);
  auto SourceCallBack = std::make_unique<ScannerSourceFileCallback>(HashCache);

  Tool.run(newFrontendActionFactory(Finder.get(), SourceCallBack.get()).get());
}

} // namespace clang::class_wrapper

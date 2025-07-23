/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#include "DeclScanner.h"
#include "Support.h"

#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Transformer/SourceCode.h"

#include <filesystem>

namespace clang::class_wrapper {

const Matcher<Decl> DeclScanner::TypedefDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, typedefDecl().bind(TypedefDeclID));
const Matcher<Decl> DeclScanner::RecordDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, recordDecl().bind(RecordDeclID));
const Matcher<Decl> DeclScanner::EnumDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, enumDecl().bind(EnumDeclID));
const Matcher<Decl> DeclScanner::VarDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, varDecl().bind(VarDeclID));
const Matcher<Decl> DeclScanner::FunctionDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource,
             functionDecl().bind(FunctionDeclID));
const Matcher<Stmt> DeclScanner::DeclStmtMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, declStmt().bind(DeclStmtID));
const Matcher<Stmt> DeclScanner::DeclRefExprMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, declRefExpr().bind(DeclRefExprID));


template <typename NodeType>
concept PrettyDumpNode =
    requires(const NodeType &Node, const ASTContext &Context)
    {
      Node.dumpPretty(Context);
    };


static Range getRangeFromChar(const CharSourceRange &CSR,
                              const SourceManager &SM) {
  unsigned Begin = SM.getFileOffset(CSR.getBegin());
  unsigned End = SM.getFileOffset(CSR.getEnd());
  return Range(SM.getFileOffset(CSR.getBegin()), End - Begin);
}


// Ignoring spaces, newlines, comments, and tabs when getting the Hash
static hash_code getTokenHash(SourceRange SR, const SourceManager &SM,
                              const CompilerInstance &CI) {
  hash_code Hash(0);

  Token Tok;
  bool ReachBegin = false;
  bool ReachedEnd = false;

  Preprocessor &PP = CI.getPreprocessor();
  SourceLocation Begin = SR.getBegin();
  SourceLocation End = SR.getEnd();

  llvm::errs() << "RawSrcRange: " << Begin.getRawEncoding() << " " << End.
      getRawEncoding() << "\n";

  SourceLocation Entrance;
  if (Begin.isMacroID()) {
    Entrance = SM.getExpansionLoc(Begin);
  } else {
    Entrance = SM.getSpellingLoc(Begin);
  }

  PP.EnterSourceFile(SM.getFileID(Entrance), nullptr, Begin);

  while (!ReachedEnd) {
    PP.Lex(Tok);
    std::string TokSpelling = PP.getSpelling(Tok);
    SourceLocation TokLoc = Tok.getLocation();
    if (Tok.is(tok::eof))
      break;
    if (TokLoc== Begin)
      ReachBegin = true;
    if (!ReachBegin) {
      llvm::errs() << "Skipping token '"
          << TokSpelling << "' at " << TokLoc.printToString(SM)
          <<" " << TokLoc.getRawEncoding() << "\n";
      continue;;
    }
    llvm::errs() << "Hash token: '" << TokSpelling << "' at "
        << TokLoc.printToString(SM) << " " << TokLoc.getRawEncoding() << "\n";
    Hash = hash_combine(Hash, TokSpelling);
    if (Tok.getLocation() == End)
      ReachedEnd = true;
  }

  llvm::errs() << "Hash:" << hash_value(Hash) << "\n";
  return Hash;
}


static bool checkExpansion(SourceRange AssociatedRange,
                           SourceLocation NameLoc, const SourceManager &SM,
                           const CompilerInstance &CI,
                           const MacroExpansionRecorder &MacroRecorder,
                           std::string &Expansion, Range Replaced) {
  SourceLocation AssociatedBegin = AssociatedRange.getBegin();
  SourceLocation AssociatedEnd = AssociatedRange.getEnd();

  llvm::errs() << std::format("AssociatedBegin: {} {}\n"
                              "AssociatedEnd: {} {}\n"
                              "NameBegin: {} {}\n",
                              AssociatedBegin.printToString(SM),
                              AssociatedBegin.isMacroID(),
                              AssociatedEnd.printToString(SM),
                              AssociatedEnd.isMacroID(),
                              NameLoc.printToString(SM), NameLoc.isMacroID());
  PresumedLoc PresumedBegin = SM.getPresumedLoc(AssociatedBegin);
  llvm::errs() << "PresumedBegin: " << PresumedBegin.getFilename()
               << ":" << PresumedBegin.getLine() << ":"
               << PresumedBegin.getColumn() << "\n";

  if (AssociatedBegin.isMacroID()) {
    SourceLocation ExpansionLoc = SM.getExpansionLoc(AssociatedBegin);
    llvm::errs() << "AssociatedBegin ExpansionLoc: "
        << ExpansionLoc.printToString(SM) << "\n";
    std::optional<StringRef> ExpansionText = MacroRecorder.getExpandedText(
        ExpansionLoc);
    if (ExpansionText) {
      llvm::errs() << "AssociatedBegin Expansion: " << *ExpansionText << "\n";
      return true;
    } else {
      llvm::errs() << "AssociatedBegin Expansion not found\n";
    }
  }

  // if (AssociatedBegin.isMacroID() || AssociatedEnd.isMacroID() || NameLoc.
  //     isMacroID()) {
  //   CharSourceRange ExpansionRange = SM.getExpansionRange(AssociatedRange);
  //   llvm::errs() << "ExpansionRange: " << ExpansionRange.getAsRange().printToString(SM) << "\n";
  //   StringRef ExpansionText = Lexer::getSourceText(ExpansionRange, SM, CI.getLangOpts());
  //   llvm::errs() << "ExpansionText: " << ExpansionText << "\n";
  //   return true;
  // }

  return false;
}


std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                      const RecordDecl &RD,
                                      const CompilerInstance &CI,
                                      const MacroExpansionRecorder &MacroRecorder) {
  // Ignore RecordDecl in local scope
  if (RD.getDeclContext()->isFunctionOrMethod()) {
    return std::nullopt;
  }

  // TODO: nested case

  const SourceManager &SM = *Result.SourceManager;
  DeclEntry D;

  D.Name = RD.getName().str();
  D.FilePath = SM.getFilename(RD.getBeginLoc()).str();
  D.Kind = RD.getKind();
  D.Storage = SC_None;
  D.isUnion = RD.isUnion();

  RD.dump();
  SourceRange SR = RD.getSourceRange();
  SR.print(llvm::errs(), SM);
  llvm::errs() << "\n";

  D.IsDefinition = RD.isCompleteDefinition();
  if (D.IsDefinition) {
    D.ImplHash = getTokenHash(SR, SM, CI);
  }

  SourceLocation NameLoc;
  if (const auto *Id = RD.getIdentifier()) {
    NameLoc = RD.getLocation();
    unsigned NameBegin = SM.getFileOffset(NameLoc);
    D.NameRange = Range(NameBegin, Id->getLength());
  } else {
    D.isAnonymous = true;
  }

  CharSourceRange AssociatedRange = getAssociatedRange(RD, *Result.Context);
  if (AssociatedRange.isInvalid()) {
    AssociatedRange = CharSourceRange::getTokenRange(SR);
  }
  D.FullRange = getRangeFromChar(AssociatedRange, SM);

  checkExpansion(SR, NameLoc, SM, CI, MacroRecorder, D.Expansion, D.ExpansionReplaced);


  return D;
}


void DeclScanner::PostHandleNode(const MatchFinder::MatchResult &Result,
                                 const RecordDecl &RD, DeclEntry &Entry) {

}

#if 0

template <typename MatcherHandler, typename NodeType, auto bindName>
class ScannerMatcherHandler : public MatchFinder::MatchCallback {
  DeclScanner &Scanner;
public:
  ScannerMatcherHandler(const NeedToWrapFunc &NeedToWrap,
                        const RecordSymbolFunc &RecordSymbol,
                        ExtendedODRHash::ODRHashCache &Cache)
      : NeedToWrap(NeedToWrap), RecordSymbol(RecordSymbol),
        TypeHashCache(Cache) {}

  void run(const MatchFinder::MatchResult &Result) override {
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

#endif


DeclScanner::DeclScanner(StringRef Target, ArrayRef<std::string> Filenames,
                         const ClassWrapperContext &Context)
  : Context(Context), Target(Target),
    SourcePaths(Filenames.begin(), Filenames.end()),
    RecordDeclHandler(*this) {
  Finder.addMatcher(RecordDeclMatcher, &RecordDeclHandler);
}


bool DeclScanner::handleBeginSource(CompilerInstance &CI) {
  CompilerInstancePtr = &CI;

  CurrentFilePath = CI.getSourceManager().getFileEntryForID(
      CI.getSourceManager().getMainFileID())->tryGetRealPathName();
  using namespace std::filesystem;
  RelativeCurrentFilePath = relative(path(CurrentFilePath),
                                     path(Context.SourceRoot)).generic_string();
  MacroContext = std::make_unique<MacroExpansionRecorder>(CI.getLangOpts());
  MacroContext->registerForPreprocessor(CI.getPreprocessor());
  return true;
}


void DeclScanner::handleEndSource() {
  // TODO: write to files
}


void DeclScanner::run(StringRef Target, ArrayRef<std::string> Filenames,
                      const CompilationDatabase &Compilations,
                      const ClassWrapperContext &Context) {
  ClangTool Tool(Compilations, Filenames,
                 std::make_shared<PCHContainerOperations>(),
                 Context.getBaseFS(), Context.getFiles());
  DeclScanner Scanner(Target, Filenames, Context);

  Tool.run(newFrontendActionFactory(&Scanner.Finder, &Scanner).get());
}

} // namespace clang::class_wrapper
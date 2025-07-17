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


static hash_code getTokenHash(CharSourceRange SCR, const SourceManager &SM,
                              const CompilerInstance &CI) {
  hash_code Hash(0);

  Token Tok;
  std::string TokensStr;
  bool ReachedEnd = false;
  Preprocessor &PP = CI.getPreprocessor();
  SourceLocation Begin = SCR.getBegin();
  SourceLocation End = SCR.getEnd();

  PP.EnterSourceFile(SM.getFileID(Begin), nullptr, Begin);

  while (!ReachedEnd) {
    PP.Lex(Tok);
    if (Tok.is(tok::eof) || SM.isBeforeInTranslationUnit(End, Tok.getLocation()))
      break;
    if (Tok.getLocation() == End)
      ReachedEnd = true;
    TokensStr += PP.getSpelling(Tok);
    TokensStr += " ";
  }

  llvm::errs() << "Tokens: " << TokensStr << "\n";

  return Hash;
}


DeclEntry::DeclEntry(const MatchFinder::MatchResult &Result,
                     const RecordDecl &RD, const CompilerInstance &CI) {
  const SourceManager& SM = *Result.SourceManager;

  Name = RD.getName().str();
  FilePath = SM.getFilename(RD.getBeginLoc()).str();
  Kind = RD.getKind();
  Storage = SC_None;

  RD.dump();
  RD.getSourceRange().print(llvm::errs(), SM);
  // TODO
  llvm::errs() << "0\n";

  if (const auto *Id = RD.getIdentifier()) {
    SourceLocation NameLoc = RD.getLocation();
    llvm::errs() << "1\n";
    unsigned NameBegin = SM.getFileOffset(NameLoc);
    llvm::errs() << "2\n";
    NameRange = Range(NameBegin, Id->getLength());
  } else {
    isAnonymous = true;
  }

  llvm::errs() << "3\n";

  CharSourceRange AssociatedRange = getAssociatedRange(RD, *Result.Context);
  FullRange = getRangeFromChar(AssociatedRange, SM);

  //getTokenHash(AssociatedRange, SM, CI);
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

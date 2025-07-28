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


static Range getRangeFromAssociated(const CharSourceRange &CSR,
                              const SourceManager &SM) {
  unsigned Begin = SM.getFileOffset(CSR.getBegin());
  unsigned End = SM.getFileOffset(CSR.getEnd());
  return Range(Begin, End - Begin);
}


// Ignoring spaces, newlines, comments, and tabs when getting the Hash
// Attention: cannot handle macro expansion
static hash_code getTokenHash(SourceLocation Begin, SourceLocation End,
                              const SourceManager &SM,
                              const CompilerInstance &CI) {
  hash_code Hash(0);

  Token Tok;

  Preprocessor &PP = CI.getPreprocessor();
  assert(Begin.isFileID());

  PP.EnterSourceFile(SM.getFileID(Begin), nullptr, Begin);

  while (true) {
    PP.Lex(Tok);
    if (Tok.is(tok::eof))
      break;
    std::string TokSpelling = PP.getSpelling(Tok);
    SourceLocation TokLoc = Tok.getLocation();
    // llvm::errs() << "Hash token: '" << TokSpelling << "' at "
    //     << TokLoc.printToString(SM) << " " << TokLoc.getRawEncoding() << "\n";
    Hash = hash_combine(Hash, TokSpelling);
    if (TokLoc == End)
      break;
  }

  // llvm::errs() << "Hash:" << hash_value(Hash) << "\n";
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


namespace {

class ExpansionTokenReader {
  std::string ExpansionText;
  SourceLocation Begin;
  SourceLocation End;
  SourceLocation TextBegin;
  SourceLocation CurrentLoc;

public:
  Token getToken(){

  }

  StringRef getText() {

  }
};

} // namespace



static std::tuple<std::string/*Expansion*/,
                  hash_code, SmallVector<unsigned, 4>/*Offsets*/>
expandOnLocations(const SmallSet<SourceLocation, 3> &ExpansionLocs,
                  SourceLocation HashBegin, SourceLocation HashEnd,
                  const SmallVectorImpl<SourceLocation> &OffsetQueries,
                  const SourceManager &SM) {
  std::string Expansion;
  hash_code Hash(0);
  SmallVector<unsigned, 4> Offsets;
}


static CharSourceRange getFullRange(SourceRange SR,
                                    const SourceManager &SM,
                                    const LangOptions &LangOpts) {
  llvm::errs() << "SourceRange: " << SR.printToString(SM) << "\n";
  CharSourceRange Range = CharSourceRange::getCharRange(SR);
  if (SR.getBegin().isMacroID())
    Range.setBegin(SM.getExpansionLoc(SR.getBegin()));
  SourceLocation End = SR.getEnd();
  if (End.isMacroID()) {
    CharSourceRange ExpansionRange = SM.getExpansionRange(SR.getEnd());
    End = ExpansionRange.getEnd();
  }

  End = Lexer::getLocForEndOfToken(End, 0, SM, LangOpts);

  std::pair<FileID, unsigned> LocInfo = SM.getDecomposedLoc(End);
  bool Invalid = false;
  StringRef Buffer = SM.getBufferData(LocInfo.first, &Invalid);

  if (Invalid) {
    Range.setEnd(End);
    llvm::errs() << "Buffer Invalid, Full Range: " << Range.getAsRange().
        printToString(SM) << "\n";
    return Range;
  }

  Token Tok;
  const char *StrData = Buffer.data() + LocInfo.second;
  Lexer TheLexer(SM.getLocForStartOfFile(LocInfo.first), LangOpts,
                 Buffer.begin(), StrData, Buffer.end());
  TheLexer.SetCommentRetentionState(true);

  while (!TheLexer.LexFromRawLexer(Tok)) {
    llvm::errs() << "Token: " << Tok.getName() << " at "
        << Tok.getLocation().printToString(SM) << "\n";
    if (Tok.is(tok::semi) || Tok.is(tok::comment)) {
      End = Tok.getEndLoc();
    } else {
      break;
    }
  }

  StrData = Buffer.data() + SM.getFileOffset(End);

  while (true) {
    if (isHorizontalWhitespace(*StrData)) {
      ++StrData;
      End = End.getLocWithOffset(1);
    } else if (isVerticalWhitespace(*StrData)) {
      ++StrData;
      End = End.getLocWithOffset(1);
      break;
    } else {
      break;
    }
  }

  Range.setEnd(End);
  llvm::errs() << "Full Range: " << Range.getAsRange().printToString(SM) <<
      "\n";
  return Range;
}



std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                      const RecordDecl &RD,
                                      const CompilerInstance &CI,
                                      const MacroExpansionRecorder &MacroRecorder) {
  // Ignore RecordDecl in local scope
  const DeclContext * DC = RD.getDeclContext();
  if (DC->isFunctionOrMethod()) {
    return std::nullopt;
  }

  // FIXME: Strictly speaking, we have to capture the nested RecordDecls.
  //  Supposing there is a `struct Inner` nested in `struct Outer`,
  //  using `struct Inner` is valid in C but only `Outer::Inner` is valid in C++
  //  Ignore it for now, since no occurrences in target codebase.
  const auto &Parents = Result.Context->getParents(RD);
  assert(Parents.size() == 1);
  if (!Parents[0].get<TranslationUnitDecl>())
    return std::nullopt;

  // TODO: nested case
  const SourceManager &SM = *Result.SourceManager;
  DeclEntry D;

  RD.dump();
  SourceRange SR = RD.getSourceRange();
  SourceLocation SBegin = SR.getBegin();
  SourceLocation SEnd = SR.getEnd();

  SR.print(llvm::errs(), SM);
  llvm::errs() << "\n";

  D.Name = RD.getName().str();
  D.IsAnonymous = D.Name.empty();
  D.FilePath = SM.getFilename(RD.getBeginLoc()).str();
  D.Kind = RD.getKind();
  D.IsUnion = RD.isUnion();
  D.IsDefinition = RD.isCompleteDefinition();
  CharSourceRange AssociatedRange = getAssociatedRange(RD, *Result.Context);

  if (SBegin.isMacroID() || SEnd.isMacroID()) {
    if (AssociatedRange.isInvalid())
      AssociatedRange = getFullRange(SR, SM, CI.getLangOpts());
    D.ExpansionReplaced = getRangeFromAssociated(AssociatedRange, SM);
    D.NeedExpansion = true;
    return D;
  }

  if (D.IsDefinition)
    D.ImplHash = getTokenHash(SBegin, SEnd, SM, CI);
  D.FullRange = getRangeFromAssociated(AssociatedRange, SM);

  SmallSet<const RecordDecl *, 8> NestedDecls;
  NestedDecls.insert(&RD);
  for (const FieldDecl *Field : RD.fields()) {
    llvm::errs() << "Field: " << Field->getName() << "\n";
    // if (const auto *RT = Field->getType()->getAs<RecordType>()) {
    //   if (const RecordDecl *NestedRD = RT->getDecl()) {
    //     if (NestedRD->getDeclContext() == &RD) {
    //       NestedDecls.insert(NestedRD);
    //     }
    //   }
    // }
  }


  return D;

  // SmallSet<SourceLocation, 2> ExpansionLocs;
  // if (SBegin.isMacroID())
  //   ExpansionLocs.insert(SM.getExpansionLoc(SBegin));
  // if (SEnd.isMacroID())
  //   ExpansionLocs.insert(SM.getExpansionLoc(SEnd));
  //
  // SourceLocation NameLoc;
  // if (const auto *Id = RD.getIdentifier()) {
  //   NameLoc = RD.getLocation();
  //   unsigned NameBegin = SM.getFileOffset(NameLoc);
  //   D.NameRange = Range(NameBegin, Id->getLength());
  //   if (NameLoc.isMacroID())
  //     ExpansionLocs.insert(SM.getExpansionLoc(NameLoc));
  // } else {
  //
  // }
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
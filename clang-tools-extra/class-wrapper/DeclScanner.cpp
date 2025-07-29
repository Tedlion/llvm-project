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
                                      const CompilerInstance &CI) {
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
  DeclEntry DE;

  RD.dump();
  SourceRange SR = RD.getSourceRange();
  SourceLocation SBegin = SR.getBegin();
  SourceLocation SEnd = SR.getEnd();

  SR.print(llvm::errs(), SM);
  llvm::errs() << "\n";

  DE.Name = RD.getName().str();
  DE.IsAnonymous = DE.Name.empty();
  DE.FilePath = SM.getFilename(RD.getBeginLoc()).str();
  DE.Kind = RD.getKind();
  DE.IsUnion = RD.isUnion();
  DE.IsDefinition = RD.isCompleteDefinition();
  CharSourceRange AssociatedRange = getAssociatedRange(RD, *Result.Context);

  if (SBegin.isMacroID() || SEnd.isMacroID()) {
    if (AssociatedRange.isInvalid())
      AssociatedRange = getFullRange(SR, SM, CI.getLangOpts());
    DE.ExpansionReplaced = getRangeFromAssociated(AssociatedRange, SM);
    DE.NeedExpansion = true;
    return DE;
  }

  if (DE.IsDefinition)
    DE.ImplHash = getTokenHash(SBegin, SEnd, SM, CI);
  DE.FullRange = getRangeFromAssociated(AssociatedRange, SM);

  llvm::errs() << "decls:\n";
  SmallSet<const RecordDecl *, 8> ClosureDecls;
  ClosureDecls.insert(&RD);
  for (const auto * TheDecl : RD.decls()) {
    if (const RecordDecl * NestedRD = dyn_cast<RecordDecl>(TheDecl))
      ClosureDecls.insert(NestedRD);
    else if (const auto * Field = dyn_cast<FieldDecl>(TheDecl)) {
      llvm::errs() << "Field: " << Field->getName() << "\n";
      const Type * FieldType = Field->getType().getTypePtr();
      // llvm::errs() << std::format("Field: {} Type: {}\n",
      //     Field->getName(), FieldType->getAsString());
    }
  }

  return DE;

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


static std::optional<std::pair<std::string, RefEntry>> getDependent(const Type& T) {

}



std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                      const TypedefDecl &TD,
                                      const CompilerInstance &CI) {
  const SourceManager &SM = *Result.SourceManager;
  TD.dump();
  // llvm::outs() << TD->getUnderlyingType().getAsString() << "\n";
  llvm::errs() << std::format("UnderlyingType: {}\n", TD.getUnderlyingType().getAsString());

  DeclEntry DE;
  DE.Name = TD.getName();
  DE.FilePath = SM.getFilename(TD.getBeginLoc()).str();
  DE.Kind = TD.getKind();
  QualType UnderlyingType = TD.getUnderlyingType();

  if (!UnderlyingType->isFunctionPointerType()) {
    DE.TypeName1 = UnderlyingType.getAsString();
    if (auto Ref = getDependent(*UnderlyingType)) {
      DE.InfRefs[Ref->first] = Ref->second;
    }
  }

  else {
    // const FunctionProtoType *ProtoType =
    //     UnderlyingType->getAs<FunctionProtoType>();
    // DE.TypeName1 = ProtoType->getReturnType().getAsString();
    // DE.TypeName2 = ProtoType->getCanonicalSignature().getAsString();
    // DE.IsFunctionPtr = true;
  }

  SourceRange SR = TD.getSourceRange();
  CharSourceRange AssociatedRange = getAssociatedRange(TD, *Result.Context);
  if (AssociatedRange.isInvalid())
    AssociatedRange = getFullRange(SR, SM, CI.getLangOpts());
  DE.FullRange = getRangeFromAssociated(AssociatedRange, SM);

  return DE;
}


void DeclScanner::PostHandleNode(const MatchFinder::MatchResult &Result,
                                 const RecordDecl &RD, DeclEntry &Entry) {

}


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
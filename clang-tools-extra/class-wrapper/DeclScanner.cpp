/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#include "DeclScanner.h"
#include "Support.h"

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/PreprocessorOutputOptions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Transformer/SourceCode.h"

#include <filesystem>
#include <fstream>
#include <print>

#define DEBUG_TYPE "class-wrapper-decl-scanner"
#define LLVM_DEBUG(x) x

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

  // FIXME: Calculate the hash of the preprocessed result
  //  The current implementation fails when the Begin is not unique
  // llvm::errs() << "Begin: " << Begin.printToString(SM) << "\n";
  assert(Begin.isFileID());

  // FIXME: now always from the start
  PP.EnterSourceFile(SM.getFileID(Begin), nullptr, Begin);

  bool BeginFound = false;
  while (true) {
    PP.Lex(Tok);
    if (Tok.is(tok::eof))
      break;
    SourceLocation TokLoc = Tok.getLocation();
    if (TokLoc == Begin)
      BeginFound = true;
    if (!BeginFound)
      continue;

    std::string TokSpelling = PP.getSpelling(Tok);
    // llvm::errs() << "Hash token: '" << TokSpelling << "' at "
    //     << TokLoc.printToString(SM) << " " << TokLoc.getRawEncoding() << "\n";
    Hash = hash_combine(Hash, TokSpelling);
    if (TokLoc == End)
      break;
  }

  // llvm::errs() << "Hash:" << hash_value(Hash) << "\n";
  return Hash;
}


#if 0
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
#endif


static CharSourceRange getFullRange(SourceRange SR,
                                    const SourceManager &SM,
                                    const LangOptions &LangOpts) {
  // llvm::errs() << "SourceRange: " << SR.printToString(SM) << "\n";
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
    // llvm::errs() << "Buffer Invalid, Full Range: " << Range.getAsRange().
    //     printToString(SM) << "\n";
    return Range;
  }

  Token Tok;
  const char *StrData = Buffer.data() + LocInfo.second;
  Lexer TheLexer(SM.getLocForStartOfFile(LocInfo.first), LangOpts,
                 Buffer.begin(), StrData, Buffer.end());
  TheLexer.SetCommentRetentionState(true);

  while (!TheLexer.LexFromRawLexer(Tok)) {
    // llvm::errs() << "Token: " << Tok.getName() << " at "
    //     << Tok.getLocation().printToString(SM) << "\n";
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
  // llvm::errs() << "Full Range: " << Range.getAsRange().printToString(SM) <<
  //     "\n";
  return Range;
}


static void printTypeRef(const std::string &Name) {
  // llvm::errs() << "get Type: " << Name << "\n";
}

namespace {
class TypeDependencyVisitor
    : public RecursiveASTVisitor<TypeDependencyVisitor> {
public:
  using ResultCallback =
    std::function<void(const std::string &, const RefEntry &)>;

  static void scanOn(QualType Type, const ResultCallback & Callback) {
    TypeDependencyVisitor Visitor(Callback);
    Visitor.TraverseType(Type);
  }

  static void scanOn(const Decl &D, const ResultCallback & Callback) {
    TypeDependencyVisitor Visitor(Callback);
    Visitor.TraverseDecl(const_cast<Decl *>(&D));
  }


  bool TraversePointerType(PointerType *PT) {
    if (hasPointeeIndependent(PT))
      return true;
    return RecursiveASTVisitor::TraversePointerType(PT);
  }


  bool TraversePointerTypeLoc(PointerTypeLoc PTL) {
    const PointerType *PT = PTL.getTypePtr();
    if (hasPointeeIndependent(PT))
      return true;
    return RecursiveASTVisitor::TraversePointerTypeLoc(PTL);
  }


  bool VisitRecordDecl(RecordDecl *RD) {
    if (RD->isCompleteDefinition()) {
      // llvm::errs() << std::format("Visiting RecordDecl: {} {}\n", RD->getName(),
      //                       static_cast<void *>(RD));
      EnclosureRecords.insert(RD);
    }
    return true;
  }


  bool VisitElaboratedType(ElaboratedType * ET) {
    // llvm::errs() << std::format("Visiting ElaboratedType: {} {}\n",
    //                         ET->getNamedType().getAsString(),
    //                         static_cast<void *>(ET));
    if (const RecordType *RT = dyn_cast<RecordType>(
        ET->getNamedType().getTypePtr()))
      if (RecordDecl *RD = RT->getDecl()) {
        if (!RD->getIdentifier())
          return true;

        // If the RecordDecl is already visited, we can skip it.
        if (EnclosureRecords.contains(RD)) {
          // llvm::errs() << std::format("Skip visiting RecordDecl: {} {}\n",
          //                             RD->getName(), static_cast<void *>(RD));
          return true;
        }
      }

    // Fixme: we ignore the difference between C and C++ here,
    //  for code `struct S{};`
    //  symbol `S` is unknown in C, only `struct S` is valid.
    //  But in C++, `S` is valid.
    //  Ignore the difference for now, since no occurrences in target codebase.
    std::string Name = ET->getNamedType().getAsString();
    if (ET->getKeyword() != ElaboratedTypeKeyword::None)
      Name = Name.substr(Name.find(' ') + 1);
    printTypeRef(Name);
    Callback(Name, RefEntry{Decl::Kind::Typedef, Range(0, 0)});
    return true;
  }

private:
  ResultCallback Callback;

  explicit TypeDependencyVisitor(const ResultCallback &CB) : Callback(CB) {}

  SmallSet<RecordDecl *, 4> EnclosureRecords;

  bool hasPointeeIndependent(const PointerType * PT) {
    QualType Pointee = PT->getPointeeType();
    const Type *PointeeType = Pointee.getTypePtr();

    if (!PointeeType)
      return true;

    // For type usage like `typedef struct S * S_t;` or `struct S * p;`,
    // the declaration of `struct S` is not necessary,
    // thus we should early return here.
    if (const auto *Elaborated = dyn_cast<ElaboratedType>(PointeeType)) {
      auto Keyword = Elaborated->getKeyword();
      if (Keyword == ElaboratedTypeKeyword::Struct ||
          Keyword == ElaboratedTypeKeyword::Union ||
          Keyword == ElaboratedTypeKeyword::Class)
        return true;
    }

    return false;
  }
};

} // namespace



std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                      const RecordDecl &RD,
                                      const CompilerInstance &CI) {
  // FIXME: Not all RecordDecl in local scope can be ignored,  for a local
  //  RecordDecl with function pointer field, an edition of transferring it to
  //  member function pointer is required.
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

  const SourceManager &SM = *Result.SourceManager;
  DeclEntry DE;

  // RD.dump();
  SourceRange SR = RD.getSourceRange();
  SourceLocation SBegin = SR.getBegin();
  SourceLocation SEnd = SR.getEnd();

  // SR.print(llvm::errs(), SM);
  // llvm::errs() << "\n";

  DE.Name = RD.getName().str();
  DE.IsUnnamed = DE.Name.empty();
  DE.FilePath = SM.getFilename(RD.getBeginLoc()).str();
  DE.Kind = RD.getKind();
  DE.RecordID = &RD;
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

  DE.FullRange = getRangeFromAssociated(AssociatedRange, SM);

  if (!DE.IsDefinition)
    return DE;

  DE.ImplHash = getTokenHash(SBegin, SEnd, SM, CI);
  TypeDependencyVisitor::scanOn(
      RD,
      [&DE](const std::string &Name, const RefEntry &Ref) {
        DE.ImplRefs.try_emplace(Name, Ref);
      });

  return DE;
}


#if 0
static std::optional<std::pair<std::string, RefEntry>> getDependent(const Type* T);

static std::optional<std::pair<std::string, RefEntry> > getDependent(
    const QualType &QT) {
  return getDependent(QT.getTypePtr());
}


static std::optional<std::pair<std::string, RefEntry>> getDependent(const Type* T) {
  if (!T)
    return std::nullopt;
  // FIXME: not working on the canonical type

  if (isa<BuiltinType>(T))
    return std::nullopt;

  if (const auto * PT= dyn_cast<PointerType>(T)) {
    const QualType Pointee = PT->getPointeeType();
    const Type * PointeeType = Pointee.getTypePtr();

    if (!PointeeType)
      return std::nullopt;

    // For type usage like `typedef struct S * S_t;` or `struct S * p;`,
    // the declaration of `struct S` is not necessary,
    // thus we should return nullopt when the Pointee is a direct RecordType.
    if (const auto *Elaborated = dyn_cast<ElaboratedType>(PointeeType)) {
      auto Keyword = Elaborated->getKeyword();
      if (Keyword == ElaboratedTypeKeyword::Struct ||
          Keyword == ElaboratedTypeKeyword::Union ||
          Keyword == ElaboratedTypeKeyword::Class)
        return std::nullopt;
    }

    return getDependent(PointeeType);
  }

  if (const auto * AT = dyn_cast<ArrayType>(T)) {
    const QualType ElementType = AT->getElementType();
    const Type * ElementTypePtr = ElementType.getTypePtr();

    if (!ElementTypePtr)
      return std::nullopt;

    return getDependent(ElementTypePtr);
  }

  if (const auto * Elaborated = dyn_cast<ElaboratedType>(T)) {
    llvm::errs() << "ElaboratedType: " << Elaborated->getNamedType().getAsString() << "\n";
    return std::make_pair(
        Elaborated->getNamedType().getAsString(),
        RefEntry{Decl::Kind::Typedef, Range(0, 0)});
  }

  return std::nullopt;
}
#endif

static void findClassnameInsertions(
    StringRef TypeName, SmallVectorImpl<EditLocation> &EditLocations) {
  // We need to turn all function pointer to member function pointer,
  // by inserting the class name specifier before the '*' which represent
  // the function pointer type.
  // There may be multiple insertion locations, for the types of a function's
  // return and parameters can also be function pointers.
  //  e.g. The type name may be `int (*(*(*[10])(int))(foo_t (**)(int)))()`
  size_t From = 0;
  while (true) {
    From = TypeName.find("(*", From);
    if (From == StringRef::npos)
      break;

    EditLocations.emplace_back(EditKind::InsertClassName, From + 1);
    From += 2;
  }
}


static EditLocation findArrayNameInsertion(StringRef TypeName) {
  return {EditKind::InsertTypedefName,
          static_cast<unsigned>(TypeName.find('[', 0))};
}


static EditLocation findFunctionPtrNameInsertion(StringRef TypeName) {
  // A function pointer typedef may be like
  // `typedef int (*(*(*name)(int))(foo_t (**)(int)))()`
  // The first part is always an identifier name which represent the final
  // return type, i.e.`int` in the above case.
  // No parentheses shall be found in the first part.
  // Then there will be continuous "(*" before the type name.
  size_t Pos = TypeName.find("(*", 0);
  do { Pos += 2; } while (TypeName[Pos] == '(');
  return {EditKind::InsertTypedefName, static_cast<unsigned>(Pos)};
}


static QualType removeArray(QualType QT) {
  while (const auto *AT = dyn_cast<ArrayType>(QT.getTypePtr())) {
    QT =  AT->getElementType();
  }
  return QT;
}


std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                      const TypedefDecl &TD,
                                      const CompilerInstance &CI) {
  // FIXME: Not all TypedefDecls in local scope can be ignored,  for a local
  //  typedef on function pointer, an edition of transferring it to member
  //  function pointer is required.
  const DeclContext * DC = TD.getDeclContext();
  if (DC->isFunctionOrMethod()) {
    return std::nullopt;
  }

  const SourceManager &SM = *Result.SourceManager;
  // TD.dump();
  // llvm::errs() << std::format("UnderlyingType: {}\n", TD.getUnderlyingType().getAsString());

  DeclEntry DE;
  DE.Name = TD.getName();
  DE.FilePath = SM.getFilename(TD.getBeginLoc()).str();
  DE.Kind = TD.getKind();

  QualType UnderlyingType = TD.getUnderlyingType();

  QualType RemoveArrayType = removeArray(UnderlyingType);
  if (const ElaboratedType * ET = dyn_cast<ElaboratedType>(RemoveArrayType.getTypePtr()))
    if (const RecordType * RT = dyn_cast<RecordType>(ET->getNamedType().getTypePtr()))
      DE.RecordID = RT->getDecl();

  DE.Expansion = UnderlyingType.getAsString();
  findClassnameInsertions(DE.Expansion, DE.EditLocations);

  // FunctionPtr or FunctionPtr Array
  bool WithFunctionPtr = !DE.EditLocations.empty();

  if (isa<ArrayType>(UnderlyingType.getTypePtr())) {
    DE.IsArray = true;
    DE.EditLocations.emplace_back(findArrayNameInsertion(DE.Expansion));
  } else if (WithFunctionPtr) {
    DE.IsFunctionPtr = true;
    DE.EditLocations.emplace_back(findFunctionPtrNameInsertion(DE.Expansion));
  }

  TypeDependencyVisitor::scanOn(
      UnderlyingType,
      [&DE](const std::string &Name, const RefEntry &Ref) {
        if (Name != DE.Name) // there is a self-ref sugar name?
          DE.ImplRefs.try_emplace(Name, Ref);
      });

  // The FullRange is the range to remove the typedef from the original source
  // Generating new does not rely on the old source.
  SourceRange SR = TD.getSourceRange();
  CharSourceRange AssociatedRange = getAssociatedRange(TD, *Result.Context);
  if (AssociatedRange.isInvalid())
    AssociatedRange = getFullRange(SR, SM, CI.getLangOpts());
  DE.FullRange = getRangeFromAssociated(AssociatedRange, SM);

  llvm::sort(DE.EditLocations);
  return DE;
}


void DeclScanner::postHandleNode(const MatchFinder::MatchResult &Result,
                                 const RecordDecl &RD, DeclEntry &Entry) {

}


DeclScanner::DeclScanner(StringRef Target, ArrayRef<std::string> Filenames,
                         const ClassWrapperContext &Context)
  : Context(Context), Target(Target),
    SourcePaths(Filenames.begin(), Filenames.end()),
    RecordDeclHandler(*this), TypedefDeclHandler(*this) {
  SourceFinder.addMatcher(RecordDeclMatcher, &RecordDeclHandler);
  SourceFinder.addMatcher(TypedefDeclMatcher, &TypedefDeclHandler);
}


bool DeclScanner::handleBeginSource(CompilerInstance &CI) {
  CompilerInstancePtr = &CI;

  CurrentFilePath = CI.getSourceManager().getFileEntryForID(
      CI.getSourceManager().getMainFileID())->tryGetRealPathName();
  using namespace std::filesystem;
  RelativeCurrentFilePath = relative(path(CurrentFilePath),
                                     path(Context.SourceRoot)).generic_string();
  // MacroContext = std::make_unique<MacroExpansionRecorder>(CI.getLangOpts());
  // MacroContext->registerForPreprocessor(CI.getPreprocessor());
  return true;
}


void DeclScanner::handleEndSource() {
  // TODO: write to files
}


static bool generatePreprocessed(const CompilationDatabase &Compilations,
                                 StringRef Filename,
                                 StringRef PreprocessedPath,
                                 StringRef DependencyPath,
                                 IntrusiveRefCntPtr<vfs::FileSystem> FS) {
  using namespace llvm::sys::fs;
  SmallString<128> PreprocessedDir(PreprocessedPath);
  sys::path::remove_filename(PreprocessedDir);
  std::error_code EC = create_directories(PreprocessedDir);
  if (EC) {
    llvm::errs() << "Error creating directories: " << EC.message() << "\n";
    return false;
  }

  raw_fd_ostream PreprocessedFile(PreprocessedPath, EC, CD_CreateAlways,
                                  FA_Write, OF_Text);
  if (EC) {
    llvm::errs() << "Error creating file: " << EC.message() << "\n";
    return false;
  }

  raw_fd_ostream DependencyFile(DependencyPath, EC, CD_CreateAlways, FA_Write,
                                OF_Text);
  if (EC) {
    llvm::errs() << "Error creating file: " << EC.message() << "\n";
    return false;
  }

  PrintPreprocessedAndDeps PDAction(DependencyFile, PreprocessedFile);
  ClangTool PDTool(Compilations, Filename.str(),
                   std::make_shared<PCHContainerOperations>(), FS);
  PDTool.run(PDAction.newFactory().get());
  return true;
}


FixedCompilationDatabase getPreprocessedCompilations(
    const CompilationDatabase &Compilations,
    StringRef OriginalPath, StringRef PreprocessedPath) {
  std::vector Commands = Compilations.getCompileCommands(OriginalPath);
  assert(Commands.size() == 1);
  CompileCommand Command = Commands.front();

  using std::filesystem::path;
  path Original = path(OriginalPath.str()).lexically_normal();
  for (auto &Arg : Command.CommandLine) {
    if (path(Arg).lexically_normal() == Original) {
      Arg = PreprocessedPath.str();
      break;
    }
  }

  return FixedCompilationDatabase(Command.Directory, Command.CommandLine);
}


void DeclScanner::run(StringRef Target, StringRef Filename,
                      const CompilationDatabase &Compilations,
                      const ClassWrapperContext &Context) {
  std::string RelativePath = Context.getRelativePath(Filename);
  std::string DependencyPath = Context.getDependencyPath(Target, RelativePath);
  std::string PreprocessedPath = Context.getPreprocessedPath(
      Target, RelativePath);
  std::string ScanResultPath = Context.getScanResultPath(Target, RelativePath);

  IntrusiveRefCntPtr<vfs::FileSystem> FS = vfs::createPhysicalFileSystem();

  if (!generatePreprocessed(Compilations, Filename, PreprocessedPath,
                            DependencyPath, FS)) {
    return;
  }

  ClangTool SourceTool(Compilations, Filename.str(),
                 std::make_shared<PCHContainerOperations>(), FS);
  DeclScanner Scanner(Target, Filename.str(), Context);

  SourceTool.run(newFrontendActionFactory(&Scanner.SourceFinder, &Scanner).get());

  FixedCompilationDatabase PPCompilations =
      getPreprocessedCompilations(Compilations, Filename, PreprocessedPath);

  ClangTool PPTool(PPCompilations, PreprocessedPath,
                 std::make_shared<PCHContainerOperations>(), FS);
  PPTool.run(newFrontendActionFactory(&Scanner.PreprocessedFinder,
                                 &Scanner).get());

  std::println("scan {} {}", Target, Filename.str());
}

} // namespace clang::class_wrapper

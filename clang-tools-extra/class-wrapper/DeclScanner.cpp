/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#include "DeclScanner.h"
#include "Support.h"
#include "ExpansionAssociatedRange.h"

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/Transformer/SourceCode.h"

#include <filesystem>
#include <print>

#define DEBUG_TYPE "class-wrapper-decl-scanner"
// #define LLVM_DEBUG(x) x

#ifdef NDEBUG
#error "suppose build with debug"
#endif

namespace clang::class_wrapper {

const Matcher<Decl> TypedefDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource,
             typedefDecl().bind(getBindID<TypedefDecl>()));
const Matcher<Decl> RecordDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource,
             recordDecl().bind(getBindID<RecordDecl>()));
const Matcher<Decl> EnumDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource,
             enumDecl().bind(getBindID<EnumDecl>()));
const Matcher<Decl> VarDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource,
             varDecl().bind(getBindID<VarDecl>()));
const Matcher<Decl> FunctionDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource,
             functionDecl().bind(getBindID<FunctionDecl>()));
// const Matcher<Stmt> DeclStmtMatcher =
//     traverse(TK_IgnoreUnlessSpelledInSource, declStmt().bind(DeclStmtID));
// const Matcher<Stmt> DeclRefExprMatcher =
//     traverse(TK_IgnoreUnlessSpelledInSource, declRefExpr().bind(DeclRefExprID));

// template <typename NodeType>
// concept PrettyDumpNode =
//     requires(const NodeType &Node, const ASTContext &Context)
//     {
//       Node.dumpPretty(Context);
//     };


decltype(DeclScanner::PPTokens)::const_iterator
DeclScanner::findTokenOrAfter(SourceLocation Loc) const {
  auto It = std::lower_bound(
      PPTokens.begin(), PPTokens.end(), Loc,
      [](const auto &Pair, const SourceLocation &Target) {
        return Pair.first < Target;
      });

  return It;
}


static Range getRangeFromAssociated(const CharSourceRange &CSR,
                                    const SourceManager &SM) {
  unsigned Begin = SM.getFileOffset(CSR.getBegin());
  unsigned End = SM.getFileOffset(CSR.getEnd());
  return Range(Begin, End - Begin);
}


// Ignoring spaces, newlines, comments, and tabs when getting the Hash
// Attention: cannot handle macro expansion
hash_code DeclScanner::getTokenHash(const Decl &D) const {
  hash_code Hash(0);
  const Preprocessor &PP = CI->getPreprocessor();
  // const SourceManager &SM = PP.getSourceManager();

  auto It = findTokenOrAfter(D.getBeginLoc());
  assert(It != PPTokens.end() && It->first == D.getBeginLoc());

  SourceLocation EndLoc = D.getEndLoc();

  for (auto &Attr : D.attrs()) {
    SourceRange AttrRange = Attr->getRange();
    if (AttrRange.isValid() && AttrRange.getEnd() > EndLoc)
      EndLoc = AttrRange.getEnd();
  }

  for (;; ++It) {
    const auto &[Loc, Tok] = *It;
    if (Tok.is(tok::comment))
      continue;
    std::string TokSpelling = PP.getSpelling(Tok);
    // llvm::errs() << "Hash token: '" << TokSpelling << "' at "
    //     << Loc.printToString(SM) << " " << Loc.getRawEncoding() << "\n";
    Hash = hash_combine(Hash, TokSpelling);
    if (Loc == EndLoc)
      break;
  }

  // Handle tail attributes
  ++It;
  for (; It != PPTokens.end(); ++It) {
    const auto &[Loc, Tok] = *It;
    if (Tok.isNot(tok::r_paren) && Tok.isNot(tok::r_square))
      break;
    std::string TokSpelling = PP.getSpelling(Tok);
    // llvm::errs() << "Hash token: '" << TokSpelling << "' at "
    //     << Loc.printToString(SM) << " " << Loc.getRawEncoding() << "\n";
    Hash = hash_combine(Hash, TokSpelling);
  }

  // llvm::errs() << "Hash:" << hash_value(Hash) << "\n";
  return Hash;
}


// We need to get a name for the EnumDecl, it may be referenced later
// Enum must have at least one EnumConstantDecl, so we "Borrow" the first
// EnumConstantDecl's name if the EnumDecl is unnamed.
// Note the EnumConstantDecl is exposed to out scope, so there won't be
// a name conflict.
static std::pair<StringRef, bool/*Borrowed*/> getNameOrBorrowed(
    const EnumDecl &ED) {
  bool Borrowed = false;
  StringRef Name = ED.getName();
  if (Name.empty()) {
    Borrowed = true;
    const EnumConstantDecl *ECD = *ED.enumerator_begin();
    Name = ECD->getName();
  }
  return {Name, Borrowed};
}


namespace {
class DependencyVisitor : public RecursiveASTVisitor<DependencyVisitor> {
public:
  using ResultCallback =
  std::function<void(const std::string &, const RefEntry &)>;


  static void scanOn(QualType Type, const ResultCallback &Callback) {
    DependencyVisitor Visitor(Callback);
    Visitor.TraverseType(Type);
  }


  static void scanOn(const Decl &D, const ResultCallback &Callback) {
    DependencyVisitor Visitor(Callback);
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

  bool VisitValueDecl(ValueDecl *VD) {
    DeclContext * DC = VD->getDeclContext();
    // avoid record local varDecl
    if (DC->isFunctionOrMethod())
      return true;

    llvm::errs() << std::format("Record ValueDecl: {} {}\n", VD->getName(),
                                static_cast<void *>(VD));
    EnclosureVars.insert(VD);
    return true;
  }



  // bool TraverseCStyleCastExpr(CStyleCastExpr *E, DataRecursionQueue *Queue = nullptr) {
  //   return RecursiveASTVisitor::TraverseCStyleCastExpr(E, Queue);
  // }


  // bool TraverseEnumConstantDecl(EnumConstantDecl *ECD) {
  //   llvm::errs() << std::format("Visiting EnumConstantDecl: {}\n",
  //                           ECD->getName());
  //   return RecursiveASTVisitor::TraverseEnumConstantDecl(ECD);
  // }


  // bool VisitTypeLoc(TypeLoc TL) {
  //   llvm::errs() << std::format("Visiting TypeLoc: {}\n",
  //                           TL.getType().getAsString());
  //   return true;
  // }


  bool VisitElaboratedType(ElaboratedType *ET) {
    // llvm::errs() << std::format("Visiting ElaboratedType: {} {}\n",
    //                         ET->getNamedType().getAsString(),
    //                         static_cast<void *>(ET));
    if (const auto *RT = dyn_cast<RecordType>(
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
    //llvm::errs() << "get Type: " << Name << "\n";
    Callback(Name, RefEntry{Decl::Kind::Typedef, Range(0, 0)});
    return true;
  }


  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    const ValueDecl *VD = DRE->getDecl();
    const DeclContext *DC = VD->getDeclContext();
    if (DC->isFunctionOrMethod())
      return true;
    if (EnclosureVars.contains(VD))
      return true;
    std::string Name;
    if (const auto *ECD = dyn_cast<EnumConstantDecl>(VD)) {
      const EnumDecl *ED = llvm::dyn_cast<EnumDecl>(ECD->getDeclContext());
      Name = getNameOrBorrowed(*ED).first;
    } else {
      Name = VD->getName().str();
    }

    llvm::errs() << "get DeclRef: " << Name << "\n";
    Callback(Name, RefEntry{VD->getKind(), Range(0, 0)});
    return true;
  }

private:
  ResultCallback Callback;

  explicit DependencyVisitor(const ResultCallback &CB) : Callback(CB) {}

  SmallSet<RecordDecl *, 4> EnclosureRecords;
  SmallSet<ValueDecl *, 4> EnclosureVars;


  bool hasPointeeIndependent(const PointerType *PT) {
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


std::optional<DeclEntry> DeclScanner::getDeclEntry(
    const MatchFinder::MatchResult &Result, const RecordDecl &RD) {
  // FIXME: Not all RecordDecl in local scope can be ignored,  for a local
  //  RecordDecl with function pointer field, an edition of transferring it to
  //  member function pointer is required.
  const DeclContext *DC = RD.getDeclContext();
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

  // SourceRange SR = RD.getSourceRange();
  // RD.dump();
  // SR.print(llvm::errs(), SM);
  // llvm::errs() << "\n";

  DE.Name = RD.getName().str();
  DE.IsUnnamed = DE.Name.empty();

  DE.Kind = RD.getKind();
  DE.RecordID = &RD;
  DE.IsUnion = RD.isUnion();
  DE.IsDefinition = RD.isCompleteDefinition();
  CharSourceRange AssociatedRange = getExpansionAssociatedRange(
      RD, *Result.Context);

  DE.SourcePath = SM.getFilename(AssociatedRange.getBegin());
  DE.ToRemove = getRangeFromAssociated(AssociatedRange, SM);
  if (DE.IsDefinition)
    DependencyVisitor::scanOn(
        RD, [&DE](const std::string &Name, const RefEntry &Ref) {
          DE.ImplRefs.try_emplace(Name, Ref);
        });

  return DE;
}


void DeclScanner::fillDeclEntry(DeclEntry &DE,
                                const MatchFinder::MatchResult &Result,
                                const RecordDecl &RD) {
  assert(DE.Name == RD.getName().str());

  if (!DE.IsDefinition)
    return;

  const SourceManager &SM = *Result.SourceManager;
  // SourceRange SR = RD.getSourceRange();
  // SourceLocation SBegin = SR.getBegin();
  // SourceLocation SEnd = SR.getEnd();

  CharSourceRange AssociatedRange = getExpansionAssociatedRange(
      RD, *Result.Context);
  assert(AssociatedRange.isValid());
  DE.AddToClass = getRangeFromAssociated(AssociatedRange, SM);
  DE.ImplHash = getTokenHash(RD);
}


#if 0
static std::optional<std::pair<std::string, RefEntry> > getDependent(
    const Type *T);

static std::optional<std::pair<std::string, RefEntry> > getDependent(
    const QualType &QT) {
  return getDependent(QT.getTypePtr());
}


static std::optional<std::pair<std::string, RefEntry> > getDependent(
    const Type *T) {
  if (!T)
    return std::nullopt;
  // FIXME: not working on the canonical type

  if (isa<BuiltinType>(T))
    return std::nullopt;

  if (const auto *PT = dyn_cast<PointerType>(T)) {
    const QualType Pointee = PT->getPointeeType();
    const Type *PointeeType = Pointee.getTypePtr();

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

  if (const auto *AT = dyn_cast<ArrayType>(T)) {
    const QualType ElementType = AT->getElementType();
    const Type *ElementTypePtr = ElementType.getTypePtr();

    if (!ElementTypePtr)
      return std::nullopt;

    return getDependent(ElementTypePtr);
  }

  if (const auto *Elaborated = dyn_cast<ElaboratedType>(T)) {
    llvm::errs() << "ElaboratedType: " << Elaborated->getNamedType().
        getAsString() << "\n";
    return std::make_pair(
        Elaborated->getNamedType().getAsString(),
        RefEntry{Decl::Kind::Typedef, Range(0, 0)});
  }

  return std::nullopt;
}
#endif

void DeclScanner::findClassnameInsertions(
    CharSourceRange TypedefRange,
    SmallVectorImpl<EditLocation> &EditLocations) const {
  // We need to turn all function pointer to member function pointer,
  // by inserting the class name specifier before the '*' which represent
  // the function pointer type.
  // There may be multiple insertion locations, for the types of a function's
  // return and parameters can also be function pointers.
  //  e.g. The type name may be `int (*(*(*[10])(int))(foo_t (**)(int)))()`
  SourceManager &SM = CI->getSourceManager();
  SourceLocation Begin = TypedefRange.getBegin();
  SourceLocation End = TypedefRange.getEnd();
  unsigned BeginOffset = SM.getFileOffset(Begin);
  bool LastIsLParen = false;

  for (auto It = findTokenOrAfter(Begin); It != PPTokens.end(); ++It) {
    const auto &[Loc, Tok] = *It;
    if (Loc > End)
      break;

    if (Tok.is(tok::l_paren)) {
      LastIsLParen = true;
      continue;
    }

    if (LastIsLParen && Tok.is(tok::star)) {
      unsigned Offset = SM.getFileOffset(Loc);
      // To insert just before the '*'
      EditLocations.emplace_back(EditKind::InsertClassName,
                                 Offset - BeginOffset);
    }

    LastIsLParen = false;

  }
}


// static EditLocation findArrayNameInsertion(StringRef TypeName) {
//   return {EditKind::InsertTypedefName,
//           static_cast<unsigned>(TypeName.find('[', 0))};
// }


// static EditLocation findFunctionPtrNameInsertion(StringRef TypeName) {
//   // A function pointer typedef may be like
//   // `typedef int (*(*(*name)(int))(foo_t (**)(int)))()`
//   // The first part is always an identifier name which represent the final
//   // return type, i.e.`int` in the above case.
//   // No parentheses shall be found in the first part.
//   // Then there will be continuous "(*" before the type name.
//   size_t Pos = TypeName.find("(*", 0);
//   do { Pos += 2; } while (TypeName[Pos] == '(');
//   return {EditKind::InsertTypedefName, static_cast<unsigned>(Pos)};
// }


static QualType removeArray(QualType QT) {
  while (const auto *AT = dyn_cast<ArrayType>(QT.getTypePtr())) {
    QT = AT->getElementType();
  }
  return QT;
}


std::optional<DeclEntry> DeclScanner::getDeclEntry(
    const MatchFinder::MatchResult &Result, const TypedefDecl &TD) {
  // FIXME: Not all TypedefDecls in local scope can be ignored,  for a local
  //  typedef on function pointer, an edition of transferring it to member
  //  function pointer is required.
  const DeclContext *DC = TD.getDeclContext();
  if (DC->isFunctionOrMethod()) {
    return std::nullopt;
  }

  const SourceManager &SM = *Result.SourceManager;
  // TD.dump();
  // llvm::errs() << std::format("UnderlyingType: {}\n", TD.getUnderlyingType().getAsString());

  DeclEntry DE;
  DE.Name = TD.getName();
  CharSourceRange AssociatedRange = getExpansionAssociatedRange(
      TD, *Result.Context);
  DE.SourcePath = SM.getFilename(AssociatedRange.getBegin());
  DE.Kind = TD.getKind();

  // Sometimes a RecordDecl is wrapped in a TypedefDecl,
  // we check that with Range and the RecordID.
  QualType UnderlyingType = TD.getUnderlyingType();
  QualType RemoveArrayType = removeArray(UnderlyingType);
  if (const ElaboratedType *ET = dyn_cast<ElaboratedType>(
      RemoveArrayType.getTypePtr()))
    if (const RecordType *RT = dyn_cast<RecordType>(
        ET->getNamedType().getTypePtr()))
      DE.RecordID = RT->getDecl();

  DE.ToRemove = getRangeFromAssociated(AssociatedRange, SM);
  return DE;
}


void DeclScanner::fillDeclEntry(
    DeclEntry &DE, const MatchFinder::MatchResult &Result,
    const TypedefDecl &TD) {
  assert(DE.Name == TD.getName());

  const SourceManager &SM = *Result.SourceManager;
  QualType UnderlyingType = TD.getUnderlyingType();
  CharSourceRange AssociatedRange = getExpansionAssociatedRange(
      TD, *Result.Context);

  DependencyVisitor::scanOn(
      UnderlyingType,
      [&DE](const std::string &Name, const RefEntry &Ref) {
        if (Name != DE.Name) // there is a self-ref sugar name?
          DE.ImplRefs.try_emplace(Name, Ref);
      });

  DE.AddToClass = getRangeFromAssociated(AssociatedRange, SM);

  // std::string Expansion = UnderlyingType.getAsString();
  findClassnameInsertions(AssociatedRange, DE.EditLocations);

  // FunctionPtr or FunctionPtr Array
  bool WithFunctionPtr = !DE.EditLocations.empty();

  if (isa<ArrayType>(UnderlyingType.getTypePtr())) {
    DE.IsArray = true;
  } else if (WithFunctionPtr) {
    DE.IsFunctionPtr = true;
  }

  llvm::sort(DE.EditLocations);
  DE.ImplHash = getTokenHash(TD);
}



std::optional<DeclEntry> DeclScanner::getDeclEntry(
    const MatchFinder::MatchResult &Result, const EnumDecl &ED) {
  const DeclContext *DC = ED.getDeclContext();
  if (DC->isFunctionOrMethod()) {
    return std::nullopt;
  }

  const SourceManager &SM = *Result.SourceManager;
  DeclEntry DE;

  auto [EnumName, Borrowed] = getNameOrBorrowed(ED);
  DE.Name = EnumName;
  DE.IsUnnamed = Borrowed;

  CharSourceRange AssociatedRange = getExpansionAssociatedRange(
      ED, *Result.Context);
  DE.SourcePath = SM.getFilename(AssociatedRange.getBegin());
  DE.Kind = ED.getKind();
  DE.ToRemove = getRangeFromAssociated(AssociatedRange, SM);

  // EnumDecl is always a definition (forward declaration is not allowed in std)
  DE.IsDefinition = true;

  DependencyVisitor::scanOn(
      ED, [&DE](const std::string &Name, const RefEntry &Ref) {
        DE.ImplRefs.try_emplace(Name, Ref);
      });

  return DE;
}


void DeclScanner::fillDeclEntry(DeclEntry &DE,
                                const MatchFinder::MatchResult &Result,
                                const EnumDecl &ED) {
  auto [Name, Borrowed] = getNameOrBorrowed(ED);
  assert(DE.Name == Name);

  const SourceManager &SM = *Result.SourceManager;
  DE.AddToClass = getRangeFromAssociated(
      getExpansionAssociatedRange(ED, *Result.Context), SM);
  DE.ImplHash = getTokenHash(ED);
}


void DeclScanner::postHandleNode(const MatchFinder::MatchResult &Result,
                                 const RecordDecl &RD, DeclEntry &Entry) {}


DeclScanner::DeclScanner(StringRef Target, const std::string &SourceFile,
                         const std::function<bool(StringRef)> &NeedWrapping)
  : Target(Target), SourceFile(SourceFile), NeedWrapping(NeedWrapping) {

  // SourceFinder.addMatcher(RecordDeclMatcher, &RecordDeclHandler);
  // SourceFinder.addMatcher(TypedefDeclMatcher, &TypedefDeclHandler);
}


bool DeclScanner::handleBeginSource(CompilerInstance &Compiler) {
  CI = &Compiler;
  MatchIndex = 0;

  const SourceManager &SM = Compiler.getSourceManager();
  CurrentFile = SM.getMainFileID();
  PPTokens.clear();

  StringRef FileName = SM.getFilename(SM.getLocForStartOfFile(CurrentFile));
  if (!FileName.ends_with(".i"))
    return true;

  // Is the tokens in source and headers necessary?
  Preprocessor &PP = Compiler.getPreprocessor();
  PP.setTokenWatcher([this](const Token &Tok) {
    SourceLocation Loc = Tok.getLocation();
    FileID FID = CI->getSourceManager().getFileID(Loc);

    if (CurrentFile != FID)
      return;

    if (!PPTokens.empty())
      assert(PPTokens.back().first < Loc);

    PPTokens.emplace_back(Loc, Tok);
  });

  // using namespace std::filesystem;
  // RelativeCurrentFilePath = relative(path(CurrentFilePath),
  //                                    path(Context.SourceRoot)).generic_string();
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

  PrintPreprocessedAndDeps PDAction(PreprocessedFile, &DependencyFile);
  ClangTool PDTool(Compilations, Filename.str(),
                   std::make_shared<PCHContainerOperations>(), FS);
  PDTool.run(PDAction.newFactory().get());
  return true;
}


static FixedCompilationDatabase getPreprocessedCompilations(
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

  Command.CommandLine.push_back("-x");
  Command.CommandLine.push_back("c");

  return FixedCompilationDatabase(Command.Directory, Command.CommandLine);
}


void scanDecls(StringRef Target, StringRef Filename,
               const CompilationDatabase &Compilations,
               const ClassWrapperContext &Context) {
  std::string RelativePath = Context.getRelativePath(Filename);
  std::string DependencyPath = Context.getDependencyPath(Target, RelativePath);
  std::string PreprocessedPath = Context.getPreprocessedPath(
      Target, RelativePath);
  std::string ScanResultPath = Context.getScanResultPath(Target, RelativePath);

  IntrusiveRefCntPtr<vfs::FileSystem> FS = vfs::createPhysicalFileSystem();
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
  DeclScanner Scanner(Target, Filename.str(),
                      std::bind_front(&ClassWrapperContext::needToWrap,
                                      &Context));
  Scanner.enableAllMatchers();

  SourceTool.run(
      newFrontendActionFactory(&Scanner.SourceFinder, &Scanner).get());

  FixedCompilationDatabase PPCompilations =
      getPreprocessedCompilations(Compilations, Filename, PreprocessedPath);

  ClangTool PPTool(PPCompilations, PreprocessedPath,
                   std::make_shared<PCHContainerOperations>(), FS);
  PPTool.run(
      newFrontendActionFactory(&Scanner.PreprocessedFinder, &Scanner).get());

  std::println("scan {} {}, MatchedDecls:{}, RecordDecls:{}",
               Target, Filename.str(), Scanner.MatchIndex,
               Scanner.DeclEntries.size());
}

} // namespace clang::class_wrapper
/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#include "DeclScanner.h"
#include "ExpansionAssociatedRange.h"
#include "Support.h"

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


static Range getRangeFromAssociated(const CharSourceRange &CSR,
                                    const SourceManager &SM) {
  unsigned Begin = SM.getFileOffset(CSR.getBegin());
  unsigned End = SM.getFileOffset(CSR.getEnd());
  return Range(Begin, End - Begin);
}


Range DeclScanner::getRangeFromSourceRange(SourceRange SR) const {
  SourceManager &SM = CI->getSourceManager();
  unsigned Begin = SM.getFileOffset(SR.getBegin());
  unsigned TokenEnd = SM.getFileOffset(SR.getEnd());
  auto It = findTokenOrAfter(SR.getEnd());
  assert(It != PPTokens.end() && It->first == SR.getEnd());
  unsigned TokenLen = It->second.getLength();
  return {Begin, TokenEnd + TokenLen - Begin};
}


SourceRange DeclScanner::getRangeWithAttributes(const Decl &Decl) const {
  SourceLocation Begin = Decl.getBeginLoc();
  SourceLocation End = Decl.getEndLoc();

  const Attr *AttrBefore = nullptr;
  const Attr *AttrAfter = nullptr;

  for (const Attr *A : Decl.attrs()) {
    SourceRange AttrRange = A->getRange();
    assert(AttrRange.isValid());
    // It seems
    if (AttrRange.getBegin() < Begin) {
      Begin = AttrRange.getBegin();
      AttrBefore = A;
    } else if (AttrRange.getEnd() > End) {
      End = AttrRange.getEnd();
      AttrAfter = A;
    }
  }

  if (AttrBefore) {
    auto It = findTokenOrAfter(Begin);
    switch (AttrBefore->getSyntax()) {
    case Attr::AS_GNU:
      assert((It - 1)->second.is(tok::l_paren));
      assert((It - 2)->second.is(tok::l_paren));
      assert((It - 3)->second.is(tok::kw___attribute));
      Begin = (It - 3)->first;
      break;
    case Attr::AS_CXX11:
    case Attr::AS_C23:
      assert((It - 1)->second.is(tok::l_square));
      assert((It - 2)->second.is(tok::l_square));
      Begin = (It - 2)->first;
      break;
    case Attr::AS_Declspec:
      assert((It - 1)->second.is(tok::l_paren));
      assert((It - 2)->second.is(tok::l_paren));
      assert((It - 3)->second.is(tok::kw___declspec));
      Begin = (It - 3)->first;
      break;
    default:
      llvm::errs() << std::format("Unhandled Attr syntax: {}\n",
                                  static_cast<int>(AttrBefore->getSyntax()));
      assert(0);
    }
  }

  if (AttrAfter) {
    auto It = findTokenOrAfter(End);
    switch (AttrAfter->getSyntax()) {
    case Attr::AS_GNU:
      assert((It + 1)->second.is(tok::r_paren));
      assert((It + 2)->second.is(tok::r_paren));
      End = (It + 2)->first;
      break;
    case Attr::AS_CXX11:
    case Attr::AS_C23:
      assert((It + 1)->second.is(tok::r_square));
      assert((It + 2)->second.is(tok::r_square));
      End = (It + 2)->first;
      break;
    case Attr::AS_Declspec:
      assert((It + 1)->second.is(tok::r_paren));
      assert((It + 2)->second.is(tok::r_paren));
      End = (It + 2)->first;
      break;
    default:
      llvm::errs() << std::format("Unhandled Attr syntax: {}\n",
                                  static_cast<int>(AttrAfter->getSyntax()));
      assert(0);
    }
  }
  return SourceRange(Begin, End);
}


// Ignoring spaces, newlines, comments, and tabs when getting the Hash
// Attention: cannot handle macro expansion
hash_code DeclScanner::getTokenHash(SourceRange SR, hash_code Init) const {
  hash_code Hash = Init;
  const Preprocessor &PP = CI->getPreprocessor();
  // const SourceManager &SM = PP.getSourceManager();

  for (const auto &[Loc, Tok] : getTokenView(SR)) {
    if (Tok.is(tok::comment))
      continue;
    std::string TokSpelling = PP.getSpelling(Tok);
    // llvm::errs() << "Hash token: '" << TokSpelling << "' at "
    //     << Loc.printToString(SM) << " " << Loc.getRawEncoding() << "\n";
    Hash = hash_combine(Hash, TokSpelling);
  // llvm::errs() << "Hash:" << hash_value(Hash) << "\n";
  }

  return Hash;
}


// Ignoring spaces, newlines, comments, and tabs when getting the Hash
// Attention: cannot handle macro expansion
hash_code DeclScanner::getTokenHash(const Decl &D) const {
  return getTokenHash(getRangeWithAttributes(D));
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


  bool TraversePointerType(PointerType *PT, bool TraverseQualifier) {
    if (hasPointeeIndependent(PT))
      return true;
    return RecursiveASTVisitor::TraversePointerType(PT, TraverseQualifier);
  }


  bool TraversePointerTypeLoc(PointerTypeLoc PTL, bool TraverseQualifier) {
    const PointerType *PT = PTL.getTypePtr();
    if (hasPointeeIndependent(PT))
      return true;
    return RecursiveASTVisitor::TraversePointerTypeLoc(PTL, TraverseQualifier);
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
    DeclContext *DC = VD->getDeclContext();
    // avoid record local varDecl
    if (DC->isFunctionOrMethod())
      return true;

    //llvm::errs() << std::format("Record ValueDecl: {} {}\n", VD->getName(),
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


  bool VisitTagType(TagType *TT) {
    ElaboratedTypeKeyword Keyword = TT->getKeyword();
    if (Keyword != ElaboratedTypeKeyword::Struct &&
        Keyword != ElaboratedTypeKeyword::Union &&
        Keyword != ElaboratedTypeKeyword::Class &&
        Keyword != ElaboratedTypeKeyword::Enum)
      return true;

    // llvm::errs() << std::format("Visiting TagType: {} {}\n",
    //                         QualType(TT, 0).getAsString(),
    //                         static_cast<void *>(TT));
    if (const auto *RT = dyn_cast<RecordType>(TT))
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
    std::string Name = QualType(TT, 0).getAsString();
    if (TT->getKeyword() != ElaboratedTypeKeyword::None)
      if (size_t Pos = Name.find(' '); Pos != std::string::npos)
        Name = Name.substr(Pos + 1);
    //llvm::errs() << "get Type: " << Name << "\n";
    Callback(Name, RefEntry{Decl::Kind::Typedef, Range(0, 0)});
    return true;
  }


  bool VisitTypedefType(TypedefType *TT) {
    const TypedefNameDecl *TD = TT->getDecl();
    if (!TD)
      return true;

    Callback(TD->getName().str(), RefEntry{TD->getKind(), Range(0, 0)});
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

    //llvm::errs() << "get DeclRef: " << Name << "\n";
    Callback(Name, RefEntry{VD->getKind(), Range(0, 0)});
    return true;
  }

private:
  ResultCallback Callback;

  explicit DependencyVisitor(const ResultCallback &CB) : Callback(CB) {}

  SmallSet<RecordDecl *, 4> EnclosureRecords;
  SmallSet<ValueDecl *, 4> EnclosureVars;


  static bool hasPointeeIndependent(const PointerType *PT) {
    QualType Pointee = PT->getPointeeType();
    const Type *PointeeType = Pointee.getTypePtr();

    if (!PointeeType)
      return true;

    // For type usage like `typedef struct S * S_t;` or `struct S * p;`,
    // the declaration of `struct S` is not necessary,
    // thus we should early return here.
    if (const auto *Elaborated = dyn_cast<TagType>(PointeeType)) {
      if (auto Keyword = Elaborated->getKeyword();
        Keyword == ElaboratedTypeKeyword::Struct ||
        Keyword == ElaboratedTypeKeyword::Union ||
        Keyword == ElaboratedTypeKeyword::Class)
        return true;
    }

    return false;
  }
};

} // namespace


static bool isFunctionPointerType(QualType QT) {
  if (QT->isPointerType()) {
    QualType Pointee = QT->getPointeeType();
    if (Pointee->isFunctionProtoType() || Pointee->isFunctionType())
      return true;
  }
  return false;
}


static void normalizeEditOffsets(SmallVectorImpl<EditLocation> &Edits,
                                 Range BaseRange) {
  if (BaseRange.getOffset() == DeclEntry::InvalidOffset)
    return;

  for (EditLocation &Edit: Edits) {
    assert(Edit.getOffset() >= BaseRange.getOffset());
    Edit = EditLocation(Edit.getEditKind(),
                        Edit.getOffset() - BaseRange.getOffset());
  }
}


static bool isOverlapped(SourceRange A, SourceRange B) {
  if (A.isInvalid() || B.isInvalid())
    return false;
  return A.getBegin() <= B.getEnd() && B.getBegin() <= A.getEnd();
}


// Overlapping ranges of combined decls:
// The clang AST is not with the same hierarchy with the standard grammar.
// There is not an AST node for init-declarator-list, instead, the Decls are
// individual nodes. There are Decls with overlapped SourceRange.
// For example, code "struct {int x, y;} s, (*fp2)(int x, int y);" will be
// parsed as one RecordDecl, one VarDecl for "s", and one VarDecl for the
// function pointer. As another instance, code "typedef struct {int x, y;} a, b;"
// will be parsed as one RecordDecl and two TypedefDecls for "a" and "b".
//
// It will not be a problem for removing overlapping ranges (removing the union)
// of Decls from the source code. But adding the Decls to the class is more
// complicated. We prefer to split the combined Decls, one declaration (end with
// semicolon) for each Decl node, unless the Decl is unnamed(RecordDecl). Unnamed
// RecordDecl can be combined with TypedefDecl or VarDecl, which appear later in
// AST. When a TypedefDecl or VarDecl with an unnamed RecordDecl is matched,
// we do an absorb action, which move the TypedefDecl or the VarDecl to the
// RecordDecl.


template <bool FromSource = false>
class EditVisitor : public RecursiveASTVisitor<EditVisitor<FromSource> > {
public:
  static void scanOn(const Expr &E, DeclScanner &Scanner,
                     SmallVectorImpl<EditLocation> &Edits) {
    static_assert(!FromSource,
                  "Edit from source is only designed for FunctionDecls");
    EditVisitor Visitor(Scanner, Edits);
    Visitor.TraverseStmt(const_cast<Expr *>(&E));
  }


  static bool scanOn(const FunctionDecl &FD, DeclScanner &Scanner,
                     SmallVectorImpl<EditLocation> &Edits) {
    EditVisitor Visitor(Scanner, Edits);
    Visitor.TraverseDecl(const_cast<FunctionDecl *>(&FD));
    return !Visitor.EncounterMacro;
  }


  static void scanOn(const RecordDecl &RD, DeclScanner &Scanner,
                     SmallVectorImpl<EditLocation> &Edits) {
    static_assert(!FromSource,
              "Edit from source is only designed for FunctionDecls");
    EditVisitor Visitor(Scanner, Edits);
    Visitor.TraverseDecl(const_cast<RecordDecl *>(&RD));
  }


  bool shouldTraversePostOrder() const {
    return true;
  }


  bool VisitRecordDecl(RecordDecl *RD) {
    LastDeclRange = RD->getSourceRange();
    return true;
  }


  // Seems no need to start a recursive traversal on other Decls?
  bool VisitFieldDecl(FieldDecl *FD) {
    auto [Head, Tail] = getIncrementalRange(FD->getSourceRange());
    Scanner.findClassnameInsertions(Tail, Edits);
    return true;
  }


  bool VisitVarDecl(VarDecl *VD) {
    auto [Head, Tail] = getIncrementalRange(VD->getSourceRange());
    Scanner.removeLinkage(Head, Edits);
    if (const Expr * InitExpr = VD->getInit())
      Tail.setEnd(InitExpr->getBeginLoc().getLocWithOffset(-1));
    Scanner.findClassnameInsertions(Tail, Edits);
    return true;
  }


  bool VisitCStyleCastExpr(CStyleCastExpr *E) {
    if (isFunctionPointerType(E->getTypeAsWritten())) {
      SourceRange TypeRange(E->getLParenLoc().getLocWithOffset(1),
                            E->getRParenLoc().getLocWithOffset(-1));
      Scanner.findClassnameInsertions(TypeRange, Edits);
    }

    return true;
  }

private:
  DeclScanner &Scanner;
  SmallVectorImpl<EditLocation> &Edits;
  SourceRange LastDeclRange;
  bool EncounterMacro = false;

  EditVisitor(DeclScanner &Scanner,
              SmallVectorImpl<EditLocation> &Edits) : Scanner(Scanner),
    Edits(Edits) {}


  // Attention: Head and Tail will be SR if SR is not combined with previous
  std::pair<SourceRange/*Head*/, SourceRange/*Tail*/> getIncrementalRange(
      SourceRange SR) {
    if (LastDeclRange.isInvalid() || !isOverlapped(LastDeclRange, SR)) {
      LastDeclRange = SR;
      return {SR, SR};
    }

    SourceRange Head, Tail;
    if (SR.getBegin() < LastDeclRange.getBegin())
      Head = SourceRange(SR.getBegin(),
                         LastDeclRange.getBegin().getLocWithOffset(-1));
    if (SR.getEnd() > LastDeclRange.getEnd())
      Tail = SourceRange(LastDeclRange.getEnd().getLocWithOffset(1),
                         SR.getEnd());

    LastDeclRange = SR;
    return {Head, Tail};
  }

};


std::optional<DeclEntry> DeclScanner::getDeclEntry(
    const MatchFinder::MatchResult &Result, const RecordDecl &RD) {
  // FIXME: Not all RecordDecl in local scope can be ignored:
  //  1) for a local RecordDecl with function pointer field, an edition of
  //  transferring it to member function pointer is required.
  //  2) a local RecordDecl may be referenced by a static local variable.
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
  DE.DeclID = &RD;
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

  if (!DE.IsDefinition) {
    DE.AddToClass = {0, 0};
    return;
  }

  if (!DE.IsUnnamed) {
    SourceManager &SM = CI->getSourceManager();
    DE.NameOffset = SM.getFileOffset(RD.getLocation());
  }

  const SourceManager &SM = *Result.SourceManager;
  DE.AddToClass = getRangeFromAssociated(
    getExpansionAssociatedRange(RD, *Result.Context), SM);
  SourceRange RangeWithAttrs = getRangeWithAttributes(RD);
  DE.ImplHash = getTokenHash(RangeWithAttrs);
  EditVisitor<>::scanOn(RD, *this, DE.EditLocations);
  llvm::sort(DE.EditLocations);
  normalizeEditOffsets(DE.EditLocations, DE.AddToClass);
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
    if (const auto *Tag = dyn_cast<TagType>(PointeeType)) {
      auto Keyword = Tag->getKeyword();
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

  if (const auto *Tag = dyn_cast<TagType>(T)) {
    std::string Name = QualType(Tag, 0).getAsString();
    if (Tag->getKeyword() != ElaboratedTypeKeyword::None)
      if (size_t Pos = Name.find(' '); Pos != std::string::npos)
        Name = Name.substr(Pos + 1);
    llvm::errs() << "TagType: " << Name << "\n";
    return std::make_pair(
      Name,
        RefEntry{Decl::Kind::Typedef, Range(0, 0)});
  }

  return std::nullopt;
}
#endif



// TODO:
//  1. Call the edit functions only when the function pointer is found.
//  2. Make the SourceRange as small as possible.
//  3. Handle editing on source case, which may fail when macro is encountered
//  in SourceRange.
void DeclScanner::findClassnameInsertions(
    SourceRange SR, SmallVectorImpl<EditLocation> &Edits) const {
  // We need to turn all function pointer to member function pointer,
  // by inserting the class name specifier before the '*' which represent
  // the function pointer type.
  // There may be multiple insertion locations, for the types of a function's
  // return and parameters can also be function pointers.
  //  e.g. The type name may be `int (*(*(*[10])(int))(foo_t (**)(int)))()`

  // Attention: Not all continuous "(*" are function pointers, e.g. `int x = (*p);`
  //  We need to exclude the range of expressions. There is no expression in
  //  TypedefDecl, RecordDecl(only for C) and FunctionDecl (only for C,
  //  excluding body).
  //  For VarDecl, we need to exclude the initializer expression.
  //  However, we need to handle the explicit cast expression like
  //  `void (*fvp)(int) = (void(*)(int))func1;` in VarDecl initializer and
  //  function body expression.
  // Caller should provide the correct SourceRange to exclude the
  // initializer or function body.
  SourceManager &SM = CI->getSourceManager();
  bool LastIsLParen = false;

  for (const auto &[Loc, Tok] : getTokenView(SR)) {
    if (Tok.is(tok::l_paren)) {
      LastIsLParen = true;
      continue;
    }
    // To insert just before the '*'
    if (LastIsLParen && Tok.is(tok::star))
      Edits.emplace_back(EditKind::InsertClassName,
                         SM.getFileOffset(Loc));
    LastIsLParen = false;
  }
}


void DeclScanner::removeLinkage(
    SourceRange SR, SmallVectorImpl<EditLocation> &Edits) const {
  SourceManager &SM = CI->getSourceManager();
  using namespace tok;
  for (const auto &[Loc, Tok] : getTokenView(SR)) {
    if (Tok.is(kw_static) || Tok.is(kw_extern) || Tok.is(kw_inline))
      Edits.emplace_back(
          EditKind::RemoveWord, SM.getFileOffset(Loc));
  }
}

DeclEntry *DeclScanner::findRefDecl(const Decl *D) {
  // used for finding the RecordDecl of a VarDecl or TypedefDecl,
  // probably be the last one, so we search backwards.
  for (auto It = DeclEntries.rbegin(); It != DeclEntries.rend(); ++It)
    if (It->DeclID == D)
      return &*It;
  return nullptr;
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
  if (const TagType *Tag = dyn_cast<TagType>(
    RemoveArrayType.getTypePtr())) {
    DE.DeclID = Tag->getDecl();
  }

  DE.ToRemove = getRangeFromAssociated(AssociatedRange, SM);
  return DE;
}


// static bool isOverlapping(SourceRange X, SourceRange Y) {
//   return X.getBegin() <= Y.getEnd() || Y.getBegin() <= X.getEnd();
// }
//
//
// static bool fullyContains(Range X, Range Y) {
//
// }


std::pair<bool/*isCombined*/, SourceRange> DeclScanner::checkCombinedDecls(
    const Decl &Decl, DeclEntry &DE) {
  // For an init-declarator-list, AST nodes with overlapping ranges are matched
  // For example, code "typedef struct {int x, y;} a, b[10], (*c)(int x);"
  // The following nodes are matched in order:
  //   1) RecordDecl with range "struct {int x, y;}"
  //   2) TypedefDecl with range "typedef struct {int x, y;} a"
  //   3) TypedefDecl with range "typedef struct {int x, y;} a, b[10]"
  //   4) TypedefDecl with range "typedef struct {int x, y;} a, b[10], (*c)(int x)"
  // If the later Decl's range fully contains the previous one's, an absorb
  // action is performed, which takes and make an incremental modification on the
  // previous AddToClass, EditLocations and ImplHash(only when with same
  // beginning location).

  SourceRange SourceRangeWithAttrs = getRangeWithAttributes(Decl);
  DeclEntry *PrevDE = getPrevDeclEntry(&DE);
  if (!PrevDE)
    return {false, SourceRangeWithAttrs};

  if (PrevDE->Kind == Decl::Kind::Record)
    return {false, SourceRangeWithAttrs};

  Range RangeWithAttrs = getRangeFromSourceRange(SourceRangeWithAttrs);
  Range PrevRange = PrevDE->AddToClass;
  assert(!RangeWithAttrs.overlapsWith(PrevRange) || RangeWithAttrs.contains(PrevRange));
  if (!RangeWithAttrs.contains(PrevRange))
    return {false, SourceRangeWithAttrs};

  PrevDE->AddToClass = {DeclEntry::InvalidOffset, 0};
  std::swap(DE.EditLocations, PrevDE->EditLocations);

  if (RangeWithAttrs.getOffset() < PrevRange.getOffset()) {
    unsigned HeadLength = PrevRange.getOffset() - RangeWithAttrs.getOffset();
    auto HeadIncrementalRange = SourceRange(
        SourceRangeWithAttrs.getBegin(),
        SourceRangeWithAttrs.getBegin().getLocWithOffset(HeadLength - 1));
    removeLinkage(HeadIncrementalRange, DE.EditLocations);
  }

  unsigned CurEndOffset = RangeWithAttrs.getOffset() + RangeWithAttrs.getLength();
  unsigned PrevEndOffset = PrevRange.getOffset() + PrevRange.getLength();
  unsigned TailLen = CurEndOffset - PrevEndOffset;
  SourceRange TailIncrementalRange;

  if (TailLen > 0) {
    TailIncrementalRange = SourceRange(
        SourceRangeWithAttrs.getEnd().getLocWithOffset(-static_cast<int>(TailLen - 1)),
        SourceRangeWithAttrs.getEnd());

    const Expr * InitExpr = nullptr;
    if (const VarDecl *VD = dyn_cast<VarDecl>(&Decl))
      InitExpr = VD->getInit();

    if (InitExpr) {
      SourceLocation InitBegin = InitExpr->getBeginLoc();
      SourceRange VarRange(TailIncrementalRange.getBegin(),
                           InitBegin.getLocWithOffset(-1));
      findClassnameInsertions(VarRange, DE.EditLocations);
      EditVisitor<>::scanOn(*InitExpr, *this, DE.EditLocations);
    } else
      findClassnameInsertions(TailIncrementalRange, DE.EditLocations);
  }

  if (RangeWithAttrs.getOffset() < PrevRange.getOffset())
    DE.ImplHash = getTokenHash(SourceRangeWithAttrs);
  else if (TailLen > 0)
    DE.ImplHash = getTokenHash(TailIncrementalRange, PrevDE->ImplHash);
  else
    DE.ImplHash = PrevDE->ImplHash;

  return {true, SourceRangeWithAttrs};
}


void DeclScanner::fillDeclEntry(DeclEntry &DE,
                                const MatchFinder::MatchResult &Result,
                                const TypedefDecl &TD) {
  assert(DE.Name == TD.getName());

  QualType UnderlyingType = TD.getUnderlyingType();
  DependencyVisitor::scanOn(
      UnderlyingType,
      [&DE](const std::string &Name, const RefEntry &Ref) {
        if (Name != DE.Name) // there is a self-ref sugar name?
          DE.ImplRefs.try_emplace(Name, Ref);
      });

  const SourceManager &SM = *Result.SourceManager;
  auto [Combined, RangeWithAttrs] = checkCombinedDecls(TD, DE);
  if (!Combined) {
    DE.AddToClass = getRangeFromAssociated(
      getExpansionAssociatedRange(TD, *Result.Context), SM);
    DE.ImplHash = getTokenHash(RangeWithAttrs);
  }

  findClassnameInsertions(RangeWithAttrs, DE.EditLocations);
  llvm::sort(DE.EditLocations);
  normalizeEditOffsets(DE.EditLocations, DE.AddToClass);
  SourceManager &CISM = CI->getSourceManager();
  DE.NameOffset = CISM.getFileOffset(TD.getLocation());

  // if (DE.DeclID) {
  //   DeclEntry *ReferTo = findRefDecl(DE.DeclID);
  //
  // }

  // std::string Expansion = UnderlyingType.getAsString();


  // FunctionPtr or FunctionPtr Array
  // bool WithFunctionPtr = !DE.EditLocations.empty();

  // if (isa<ArrayType>(UnderlyingType.getTypePtr())) {
  //   DE.IsArray = true;
  // } else if (WithFunctionPtr) {
  //   DE.IsFunctionPtr = true;
  // }
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


std::optional<DeclEntry> DeclScanner::getDeclEntry(
    const MatchFinder::MatchResult &Result, const VarDecl &VD) {
  const DeclContext *DC = VD.getDeclContext();
  if (DC->isFunctionOrMethod() && !VD.isStaticLocal()) {
    return std::nullopt;
  }

  const SourceManager &SM = *Result.SourceManager;
  DeclEntry DE;

  DE.Name = VD.getName();
  DE.IsStatic = VD.getStorageClass() == SC_Static;
  DE.Kind = VD.getKind();
  DE.IsDefinition = VD.hasInit();
  DE.IsExtern = VD.getStorageClass() == SC_Extern;

  CharSourceRange AssociatedRange = getExpansionAssociatedRange(
      VD, *Result.Context);
  DE.SourcePath = SM.getFilename(AssociatedRange.getBegin());
  DE.ToRemove = getRangeFromAssociated(AssociatedRange, SM);

  return DE;
}


void DeclScanner::fillDeclEntry(DeclEntry &DE,
                                const MatchFinder::MatchResult &Result,
                                const VarDecl &VD) {
  assert(DE.Name == VD.getName());

  // Note: for the code "int x, y;", VarDecl "x" and VarDecl "y" are two
  // individual nodes which do not have a "VarDeclList" parent.
  // This means the AssociatedRange for later VarDecl contains the previous ones,
  // which is acceptable for ToRemove but not for AddToClass.
  // As a consequence, we need to construct the AddToClass range manually.
  QualType VarType = VD.getType();
  std::string TypeName = VarType.getAsString();
  if (VD.hasInit()) {
    std::string InitText = tooling::getText(
      CharSourceRange::getTokenRange(VD.getInit()->getSourceRange()),
      *Result.Context).str();
    DE.AddToClassText = std::format("{} {} = {};\n", TypeName, DE.Name,
                                    InitText);
  } else {
    DE.AddToClassText = std::format("{} {};\n", TypeName, DE.Name);
  }
  //llvm::errs() << DE.AddToClassText;

  // const SourceManager &SM = *Result.SourceManager;
  // CharSourceRange AssociatedRange = getExpansionAssociatedRange(
  //     VD, *Result.Context);
  // DE.AddToClass = getRangeFromAssociated(AssociatedRange, SM);

  DependencyVisitor::scanOn(
      VD, [&DE](const std::string &Name, const RefEntry &Ref) {
        DE.ImplRefs.try_emplace(Name, Ref);
      });

  DE.ImplHash = getTokenHash(VD);
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

  // StringRef FileName = SM.getFilename(SM.getLocForStartOfFile(CurrentFile));
  // if (!FileName.ends_with(".i"))
  //   return true;

  // Is the tokens in source and headers necessary?
  Preprocessor &PP = Compiler.getPreprocessor();
  PP.setTokenWatcher([this](const Token &Tok) {
    SourceLocation Loc = Tok.getLocation();
    FileID FID = CI->getSourceManager().getFileID(Loc);

    if (CurrentFile != FID) {
      // FIXME: Add an unknown Tok to mark there is something expanded from a macro?
      return;
    }

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


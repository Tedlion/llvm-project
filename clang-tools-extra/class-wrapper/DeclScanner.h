/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLASS_WRAPPER_DECLSCANNER_H
#define LLVM_CLANG_TOOLS_EXTRA_CLASS_WRAPPER_DECLSCANNER_H

#include "ClassWrapperContext.h"

#include "MacroExpansionRecorder.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/ArrayRef.h"

namespace clang::class_wrapper {
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;
using internal::Matcher;


struct StringDenseMapInfo {
  static std::string getEmptyKey() {
    // making it strange enough
    return "e%M^p&T*y(K)e_Y";
  }

  static std::string getTombstoneKey() {
    return "t!O@m#BsToNe";
  }

  static unsigned getHashValue(const std::string &Val) {
    return llvm::hash_value(Val);
  }

  static unsigned getHashValue(StringRef Val) {
    return llvm::hash_value(Val);
  }


  static bool isEqual(const std::string & LHS, const std::string & RHS) {
    return LHS == RHS;
  }

  static bool isEqual(StringRef LHS, const std::string & RHS) {
    return LHS == RHS;
  }

  static bool isEqual(const std::string & LHS, StringRef RHS) {
    return LHS == RHS;
  }
};


enum class EditKind {
  Invalid,
  InsertClassName,
  InsertTypedefName,
  InsertVarDeclName,
};

class EditLocation {
  unsigned Edit           : 4; // EditKind
  unsigned Offset         : 28;

public:
  EditLocation(EditKind Kind, unsigned Offset)
      : Edit(static_cast<unsigned>(Kind)), Offset(Offset) {}

  EditLocation()
    : EditLocation(EditKind::Invalid, 0) {
  }

  EditKind getEditKind() const {
    return static_cast<EditKind>(Edit);
  }

  unsigned getOffset() const {
    return Offset;
  }

  bool isValid() const {
    return Edit != static_cast<unsigned>(EditKind::Invalid);
  }

  auto operator<=>(const EditLocation &Other) const {
    if (Offset == Other.Offset)
      return Edit <=> Other.Edit;
    return Offset <=> Other.Offset;
  }

  bool operator==(const EditLocation &Other) const = default;
};

struct RefEntry {
  Decl::Kind Kind;

  // points to the SameRange of DeclEntry, only set when UsedAsFunctionPtr
  Range Range;

  unsigned FailedToLocate        : 1 = false; // Failed to locate the symbol in the source code
  unsigned IsStatic              : 1 = false;
  unsigned UsedAsFunctionPtr     : 1 = false;
  unsigned Nested                : 1 = false; // Dependent type is nested in struct
};


struct DeclEntry {
  std::string Name;
  std::string FilePath; // source file's path relative to SourceRoot
  Decl::Kind Kind;

  // The Expansion is only necessary when:
  // 1. Larger than the Decl, or
  // 2. Contains the Decl Name
  // Then the expansion will be the string within AssociatedRange of the Decl
  std::string Expansion;
  Range ExpansionReplaced{0, 0};

  // sorted by Offset
  SmallVector<EditLocation, 4> EditLocations;

  // If the Expansion is empty, the following Ranges points to the sources;
  // otherwise, the Ranges points to the Expansion.
  Range NameRange{0, 0};
  Range InfRange{0, 0}; // used in function
  Range FullRange{0, 0};

  hash_code InfHash{0};   // for function only
  hash_code ImplHash{0};

  // use for retrieving the RecordDecl of type on TypedefDecl and VarDecl
  // Note: only used for comparing the pointers, never dereferencing it.
  const RecordDecl * RecordID = nullptr;

  unsigned IsStatic       : 1 = false; // for functions and variables only
  unsigned NeedExpansion  : 1 = false; // Fails to expand the macro
  unsigned IsDefinition   : 1 = false;
  unsigned IsInline       : 1 = false; // for function only
  unsigned IsUnnamed      : 1 = false; // for record only
  unsigned IsUnion        : 1 = false;
  unsigned IsArray        : 1 = false;
  unsigned IsFunctionPtr  : 1 = false;

  using MapType = SmallDenseMap<std::string, RefEntry, 4, StringDenseMapInfo>;
  MapType InfRefs;  // the symbols used in the declaration
  MapType ImplRefs; // the symbols used only in the definition

  bool operator==(const DeclEntry &) const = default;
};


std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                      const RecordDecl &RD,
                                      const CompilerInstance &CI);

std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                      const TypedefDecl &TD,
                                      const CompilerInstance &CI);


// extern hash_code getTokenHash


class DeclScanner : public SourceFileCallbacks {
public:
  // make the Matchers public for unit tests
  static constexpr char TypedefDeclID[] = "typedefDecl";
  static const Matcher<Decl> TypedefDeclMatcher;
  static constexpr char RecordDeclID[] = "recordDecl";
  static const Matcher<Decl> RecordDeclMatcher;
  static constexpr char EnumDeclID[] = "enumDecl";
  static const Matcher<Decl> EnumDeclMatcher;
  static constexpr char VarDeclID[] = "varDecl";
  static const Matcher<Decl> VarDeclMatcher;
  static constexpr char FunctionDeclID[] = "functionDecl";
  static const Matcher<Decl> FunctionDeclMatcher;
  static constexpr char DeclStmtID[] = "declStmt";
  static const Matcher<Stmt> DeclStmtMatcher;
  static constexpr char DeclRefExprID[] = "declRefExpr";
  static const Matcher<Stmt> DeclRefExprMatcher;

  static void run(StringRef Target, ArrayRef<std::string> Filenames,
                    const CompilationDatabase &Compilations,
                    const ClassWrapperContext &Context);

  // Implemation of interface of SourceFileCallbacks
  bool handleBeginSource(CompilerInstance &CI) override;
  void handleEndSource() override;

  void postHandleNode(const MatchFinder::MatchResult &Result,
                      const RecordDecl &RD, DeclEntry &Entry);

  template <std::derived_from<Decl> NodeType>
  void handleNode(const MatchFinder::MatchResult &Result, const NodeType &Node) {
    if (auto Entry = getDeclEntry(Result, Node, *CompilerInstancePtr)) {
      DeclEntries.push_back(std::move(*Entry));
      postHandleNode(Result, Node, DeclEntries.back());
    }
  }

  template <typename NodeType, const char * BindID>
  class MatchHandler : public MatchFinder::MatchCallback {
    DeclScanner &Scanner;

  public:
    MatchHandler(DeclScanner &Scanner) : Scanner(Scanner) {
    }

    void run(const MatchFinder::MatchResult &Result) override {
      const NodeType *Node = Result.Nodes.getNodeAs<NodeType>(BindID);
      if (!Node) {
        llvm::errs() << "Failed to get node as " << BindID << "\n";
        return;
      }

      StringRef FileName = Result.SourceManager->getFilename(
          Node->getBeginLoc());
      if (!Scanner.Context.needToWrap(FileName)) {
        return;
      }

      Scanner.handleNode(Result, *Node);
    }
  };

private:
  const ClassWrapperContext &Context;
  std::string Target;
  std::vector<std::string> SourcePaths;
  MatchFinder Finder;

  std::string CurrentFilePath;
  std::string RelativeCurrentFilePath; // relative to SourceRoot
  std::vector<DeclEntry> DeclEntries;

  const CompilerInstance * CompilerInstancePtr = nullptr;
  std::unique_ptr<MacroExpansionRecorder> MacroContext;

  DeclScanner(StringRef Target, ArrayRef<std::string> Filename,
                    const ClassWrapperContext &Context);

  MatchHandler<RecordDecl, RecordDeclID> RecordDeclHandler;

};

} // namespace clang::class_wrapper

template <>
struct std::formatter<clang::class_wrapper::EditLocation> :
public std::formatter<std::string> {
  auto format(const clang::class_wrapper::EditLocation& Loc, std::format_context& Ctx) const {
    std::string Result = std::format("EditKind: {}, Offset: {}",
        static_cast<int>(Loc.getEditKind()), Loc.getOffset());
    return std::formatter<std::string>::format(Result, Ctx);
  }
};

#endif // LLVM_CLANG_TOOLS_EXTRA_CLASS_WRAPPER_DECLSCANNER_H

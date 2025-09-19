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

#include <vector>
#include <ranges>

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
  RemoveWord,        // remove a word (such as "static") and following spaces
  // InsertNamePrefix,
  // InsertTypedefName,
  // InsertVarDeclName,
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
  static constexpr unsigned InvalidOffset = static_cast<unsigned>(-1);
  std::string Name;
  Decl::Kind Kind;

  std::string SourcePath; // source file's path, where to perform the ToRemove

  // // The Expansion is only necessary when:
  // // 1. Larger than the Decl, or
  // // 2. Contains the Decl Name
  // // Then the expansion will be the string within AssociatedRange of the Decl
  // std::string Expansion;
  // Range ExpansionReplaced{0, 0};

  //
  // // If the Expansion is empty, the following Ranges points to the sources;
  // // otherwise, the Ranges points to the Expansion.
  // Range NameRange{0, 0};
  // Range InfRange{0, 0}; // used in function
  // Range FullRange{0, 0};

  Range ToRemove{InvalidOffset, 0};   // range from source file
  Range AddToClass{InvalidOffset, 0}; // range of the preprocessed file
  unsigned NameOffset = InvalidOffset;

  std::string AddToClassText; // text to add to the class

  // sorted by Offset
  SmallVector<EditLocation, 4> EditLocations;

  hash_code InfHash{0};   // for function only
  hash_code ImplHash{0};

  // use for retrieving the last Decl
  //
  // RecordDecl/EnumDecl of type on TypedefDecl and VarDecl
  // Note: only used for comparing the pointers, never dereferencing it.
  const Decl * DeclID = nullptr;

  unsigned IsStatic       : 1 = false; // for functions and variables only
  // unsigned NeedExpansion  : 1 = false; // Fails to expand the macro
  unsigned IsDefinition   : 1 = false; // Strong definition for vars
  unsigned IsInline       : 1 = false; // for function only
  unsigned IsUnnamed      : 1 = false; // for record only
  unsigned IsUnion        : 1 = false;
  unsigned IsArray        : 1 = false;
  unsigned IsFunctionPtr  : 1 = false;
  unsigned IsExtern       : 1 = false;


  using MapType = SmallDenseMap<std::string, RefEntry, 4, StringDenseMapInfo>;
  MapType InfRefs;  // the symbols used in the declaration
  MapType ImplRefs; // the symbols used only in the definition

  bool operator==(const DeclEntry &) const = default;
};


extern const Matcher<Decl> TypedefDeclMatcher;
extern const Matcher<Decl> RecordDeclMatcher;
extern const Matcher<Decl> EnumDeclMatcher;
extern const Matcher<Decl> VarDeclMatcher;
extern const Matcher<Decl> FunctionDeclMatcher;
// extern const Matcher<Stmt> DeclStmtMatcher;
// extern const Matcher<Stmt> DeclRefExprMatcher;

template <typename NodeType>
constexpr const char *getBindID() = delete;


template <>
constexpr const char *getBindID<TypedefDecl>() { return "typedefDecl"; }


template <>
constexpr const char *getBindID<RecordDecl>() { return "recordDecl"; }


template <>
constexpr const char *getBindID<EnumDecl>() { return "enumDecl"; }


template <>
constexpr const char *getBindID<VarDecl>() { return "varDecl"; }


template <>
constexpr const char *getBindID<FunctionDecl>() { return "functionDecl"; }


template <typename NodeType>
const Matcher<Decl> &getMatcher();


template <>
inline const Matcher<Decl> &getMatcher<TypedefDecl>() {
  return TypedefDeclMatcher;
}


template <>
inline const Matcher<Decl> &getMatcher<RecordDecl>() {
  return RecordDeclMatcher;
}


template <>
inline const Matcher<Decl> &getMatcher<EnumDecl>() {
  return EnumDeclMatcher;
}


template <>
inline const Matcher<Decl> &getMatcher<VarDecl>() {
  return VarDeclMatcher;
}


template <>
inline const Matcher<Decl> &getMatcher<FunctionDecl>() {
  return FunctionDeclMatcher;
}


class DeclScanner : public SourceFileCallbacks {
public:
  DeclScanner() :NeedWrapping([](StringRef) { return true; }) { }

  DeclScanner(StringRef Target, const std::string& SourceFile,
    const std::function<bool(StringRef)> & );

  static void run(StringRef Target, StringRef Filename,
                  const CompilationDatabase &Compilations,
                  const ClassWrapperContext &Context);

  // Implementation of interface of SourceFileCallbacks
  bool handleBeginSource(CompilerInstance &Compiler) override;
  void handleEndSource() override;

  void postHandleNode(const MatchFinder::MatchResult &Result,
                      const RecordDecl &RD, DeclEntry &Entry);


  template <std::derived_from<Decl> NodeType>
  void handleNode(const MatchFinder::MatchResult &Result,
                  const NodeType &Node) {
    if (auto Entry = getDeclEntry(Result, Node)) {
      DeclEntries.push_back(std::move(*Entry));
      // postHandleNode(Result, Node, DeclEntries.back());
    }
  }

  template <typename NodeType>
  class SourceMatchHandler : public MatchFinder::MatchCallback {
    DeclScanner &Scanner;

  public:
    SourceMatchHandler(DeclScanner &Scanner) : Scanner(Scanner) {}

    void run(const MatchFinder::MatchResult &Result) override {
      Scanner.onSourceMatch<NodeType>(Result);
    }
  };


  template <typename NodeType>
  class PPMatchHandler : public MatchFinder::MatchCallback {
    DeclScanner &Scanner;

  public:
    PPMatchHandler(DeclScanner &Scanner) : Scanner(Scanner) {}

    void run(const MatchFinder::MatchResult &Result) override {
      Scanner.onPPMatch<NodeType>(Result);
    }
  };

  template<typename NodeType>
  void enableMatcher() {
    auto Handler = std::make_unique<SourceMatchHandler<NodeType>>(*this);
    SourceFinder.addMatcher(getMatcher<NodeType>(), Handler.get());
    MatchHandlers.push_back(std::move(Handler));

    auto PPHandler = std::make_unique<PPMatchHandler<NodeType>>(*this);
    PreprocessedFinder.addMatcher(getMatcher<NodeType>(), PPHandler.get());
    MatchHandlers.push_back(std::move(PPHandler));
  }

  void enableAllMatchers() {
    enableMatcher<TypedefDecl>();
    enableMatcher<RecordDecl>();
    enableMatcher<EnumDecl>();
    enableMatcher<VarDecl>();
  }

  [[nodiscard]] const std::vector<DeclEntry> &getDeclEntries() const {
    return DeclEntries;
  }

  MatchFinder &getSourceFinder() {
    return SourceFinder;
  }

  MatchFinder &getPreprocessedFinder() {
    return PreprocessedFinder;
  }

private:
  // const ClassWrapperContext &Context;
  std::string Target;
  std::string SourceFile;
  const std::function<bool(StringRef)> NeedWrapping;
  unsigned MatchIndex = 0;

  MatchFinder SourceFinder;
  MatchFinder PreprocessedFinder;

  FileID CurrentFile;
  // std::string CurrentFilePath;
  // std::string RelativeCurrentFilePath; // relative to SourceRoot
  std::vector<DeclEntry> DeclEntries;
  // indices of recorded DeclEntries in first scanning
  std::vector<unsigned> EntryIndices;
  unsigned CurrentEntryIndex = 0;

  const CompilerInstance *CI = nullptr;
  std::unique_ptr<MacroExpansionRecorder> MacroContext;

  std::vector<std::unique_ptr<MatchFinder::MatchCallback>> MatchHandlers;
  std::vector<std::pair<SourceLocation, Token>> PPTokens;

  auto findTokenOrAfter(SourceLocation Loc) const {
    auto It = std::lower_bound(
        PPTokens.begin(), PPTokens.end(), Loc,
        [](const auto &Pair, const SourceLocation &Target) {
          return Pair.first < Target;
        });

    return It;
  }

  auto getTokenView(SourceRange SR) const {
    auto ItBegin = findTokenOrAfter(SR.getBegin());
    SourceLocation EndLoc = SR.getEnd();

    return std::ranges::subrange(ItBegin, PPTokens.cend()) |
           std::views::take_while([EndLoc](const auto &pair) {
             return pair.first <= EndLoc;
           });
  }

  std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                        const RecordDecl &RD);
  std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                        const TypedefDecl &TD);
  std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                        const EnumDecl &ED);
  std::optional<DeclEntry> getDeclEntry(const MatchFinder::MatchResult &Result,
                                        const VarDecl &VD);

  void fillDeclEntry(DeclEntry &DE, const MatchFinder::MatchResult &Result,
                     const RecordDecl &RD);
  void fillDeclEntry(DeclEntry &DE, const MatchFinder::MatchResult &Result,
                     const TypedefDecl &TD);
  void fillDeclEntry(DeclEntry &DE, const MatchFinder::MatchResult &Result,
                     const EnumDecl &ED);
  void fillDeclEntry(DeclEntry &DE, const MatchFinder::MatchResult &Result,
                     const VarDecl &VD);

  template <typename NodeType>
  void onSourceMatch(const MatchFinder::MatchResult &Result) {
    MatchIndex++;
    const NodeType *Node =
        Result.Nodes.getNodeAs<NodeType>(getBindID<NodeType>());
    if (!Node) {
      llvm::errs() << "Failed to get node as " << getBindID<NodeType>() << "\n";
      return;
    }

    StringRef FileName = Result.SourceManager->getFilename(Node->getBeginLoc());
    if (!NeedWrapping(FileName))
      return;

    if (auto Entry = getDeclEntry(Result, *Node)) {
      DeclEntries.push_back(std::move(*Entry));
      EntryIndices.push_back(MatchIndex);
    }
  }


  template <typename NodeType>
  void onPPMatch(const MatchFinder::MatchResult &Result) {
    MatchIndex++;
    if (CurrentEntryIndex >= EntryIndices.size())
      return;
    if (EntryIndices[CurrentEntryIndex] != MatchIndex)
      return;

    const NodeType *Node =
        Result.Nodes.getNodeAs<NodeType>(getBindID<NodeType>());
    fillDeclEntry(DeclEntries[CurrentEntryIndex], Result, *Node);
    CurrentEntryIndex++;
  }

  hash_code getTokenHash(const Decl& D) const;

  hash_code getTokenHash(SourceRange SR, hash_code Init = hash_code(0)) const;

  void findClassnameInsertions(
      SourceRange SR, SmallVectorImpl<EditLocation> &EditLocations) const;

  void removeLinkage(
      SourceRange SR, SmallVectorImpl<EditLocation> &EditLocations) const;

  DeclEntry *findRefDecl(const Decl *D);

  DeclEntry *getPrevDeclEntry(DeclEntry *DE) const {
    if (DE == &DeclEntries[0])
      return nullptr;
    return DE - 1;
  }

  SourceRange getRangeWithAttributes(const Decl &Decl) const;

  /**
   * @return If the Decl is combined with previous Decl(s), return the
   * incremental range which need further check for edits; otherwise return
   * the same as Decl's range.
   */
  std::pair<bool/*isCombined*/, SourceRange> checkCombinedDecls(
      const Decl &Decl, DeclEntry &DE);

  Range getRangeFromSourceRange(SourceRange SR) const;
};


class PrintPreprocessedAndDeps : public PreprocessorFrontendAction {
public:
  PrintPreprocessedAndDeps(raw_ostream &Preprocessed, raw_ostream *Dependencies = nullptr)
    : Dependencies(Dependencies), Preprocessed(Preprocessed) {}

  void ExecuteAction() override {
    assert(!Entered && "ExecuteAction should be called only once");
    Entered = true;

    CompilerInstance &CI = getCompilerInstance();

    PreprocessorOutputOptions Opts;
    Opts.ShowCPP = true;
    Opts.KeepSystemIncludes = true;
    Opts.ShowLineMarkers = false;
    Opts.ShowComments = true;

    if (Dependencies) {
      Collector = std::make_unique<DependencyCollector>();
      Collector->attachToPreprocessor(CI.getPreprocessor());
    }

    DoPrintPreprocessedInput(CI.getPreprocessor(), &Preprocessed, Opts);

    if (Dependencies) {
      for (const auto &Dep : Collector->getDependencies()) {
        *Dependencies << Dep << "\n";
      }
    }
  }


  class Factory : public FrontendActionFactory {
    raw_ostream &Preprocessed;
    raw_ostream *Dependencies;

  public:
    explicit Factory(raw_ostream &Preprocessed, raw_ostream *Dependencies = nullptr)
      : Preprocessed(Preprocessed), Dependencies(Dependencies) {}


    std::unique_ptr<FrontendAction> create() override {
      return std::make_unique<PrintPreprocessedAndDeps>(
          Preprocessed, Dependencies);
    }
  };


  std::unique_ptr<FrontendActionFactory> newFactory() const {
    return std::make_unique<Factory>(Preprocessed, Dependencies);
  }

private:
  std::unique_ptr<DependencyCollector> Collector;
  raw_ostream *Dependencies;
  raw_ostream &Preprocessed;
  bool Entered = false;
};


} // namespace clang::class_wrapper

template <>
struct std::formatter<clang::class_wrapper::EditLocation> :
std::formatter<std::string> {
  auto format(const clang::class_wrapper::EditLocation& Loc, std::format_context& Ctx) const {
    std::string Result = std::format("EditKind: {}, Offset: {}",
        static_cast<int>(Loc.getEditKind()), Loc.getOffset());
    return std::formatter<std::string>::format(Result, Ctx);
  }
};




#endif // LLVM_CLANG_TOOLS_EXTRA_CLASS_WRAPPER_DECLSCANNER_H

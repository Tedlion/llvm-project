/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#ifndef DECLSCANNER_H
#define DECLSCANNER_H

#include "ClassWrapperContext.h"

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

struct RefEntry {
  std::string Name;
  Decl::Kind Kind;
  StorageClass Storage = SC_Extern;

  // points to the SameRange of DeclEntry, only set when UsedAsFunctionPtr
  Range Range;

  bool UsedAsFunctionPtr = false;
};


struct DeclEntry {
  std::string Name;
  std::string FilePath; // source file's path relative to SourceRoot
  Decl::Kind Kind;
  StorageClass Storage;

  std::string Expansion;

  // If the Expansion is empty, the following Ranges points to the sources;
  // otherwise, the Ranges points to the Expansion.
  Range NameRange;
  Range InfRange; // used in function
  Range FullRange;

  hash_code InfHash;   // for function only
  hash_code ImplHash;

  uint64_t IsDefinition   : 1;
  uint64_t IsInline       : 1 = false; // for function only
  uint64_t isAnonymous    : 1 = false; // for record only

  SmallVector<RefEntry, 4> InfRefs;  // the symbols used in the declaration
  SmallVector<RefEntry, 4> ImplRefs; // the symbols used in the definition

  DeclEntry(const MatchFinder::MatchResult &Result, const RecordDecl &RD,
            const CompilerInstance &CI);
  DeclEntry(const MatchFinder::MatchResult &Result, const EnumDecl &ED,
            const CompilerInstance &CI);

  bool operator==(const DeclEntry &) const = default;
};


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

  void PostHandleNode(const MatchFinder::MatchResult &Result,
                      const RecordDecl &RD, DeclEntry &Entry);

  template <std::derived_from<Decl> NodeType>
  void HandleNode(const MatchFinder::MatchResult &Result, const NodeType &Node) {
    DeclEntries.emplace_back(Result, Node, *CompilerInstancePtr);
    PostHandleNode(Result, Node, DeclEntries.back());
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

      Scanner.HandleNode(Result, *Node);
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

  DeclScanner(StringRef Target, ArrayRef<std::string> Filename,
                    const ClassWrapperContext &Context);

  MatchHandler<RecordDecl, RecordDeclID> RecordDeclHandler;

};

} // namespace clang::class_wrapper

#endif // DECLSCANNER_H

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
  const Decl::Kind Kind;
  StorageClass Storage;

  std::string Expansion;

  // If the Expansion is empty, the following Ranges points to the sources;
  // otherwise, the Ranges points to the Expansion.
  Range NameRange;
  Range InfRange;
  Range ImplRange;

  hash_code InfHash;
  hash_code ImplHash;

  bool IsDefinition;
  bool IsInline = false;
  SmallVector<RefEntry, 4> InfRefs;  // the symbols used in the declaration
  SmallVector<RefEntry, 4> ImplRefs; // the symbols used in the definition

  DeclEntry(const MatchFinder::MatchResult &Result, const RecordDecl & RD);
  DeclEntry(const MatchFinder::MatchResult &Result, const EnumDecl & ED);
};



class DeclScanner : public SourceFileCallbacks {
public:
  // make the Matchers public for unit tests
  static constexpr const char *TypedefDeclID = "typedefDecl";
  static const Matcher<Decl> TypedefDeclMatcher;
  static constexpr const char *RecordDeclID = "recordDecl";
  static const Matcher<Decl> RecordDeclMatcher;
  static constexpr const char * EnumDeclID = "enumDecl";
  static const Matcher<Decl> EnumDeclMatcher;
  static constexpr const char * VarDeclID = "varDecl";
  static const Matcher<Decl> VarDeclMatcher;
  static constexpr const char * FunctionDeclID = "functionDecl";
  static const Matcher<Decl> FunctionDeclMatcher;
  static constexpr const char * DeclStmtID = "declStmt";
  static const Matcher<Stmt> DeclStmtMatcher;
  static constexpr const char * DeclRefExprID = "declRefExpr";
  static const Matcher<Stmt> DeclRefExprMatcher;

  static void run(StringRef Target, ArrayRef<std::string> Filenames,
                    const CompilationDatabase &Compilations,
                    const ClassWrapperContext &Context);

  // Implemation of interface of SourceFileCallbacks
  bool handleBeginSource(CompilerInstance &CI) override;
  void handleEndSource() override;

  template <typename NodeType>
  void HandleNode(const MatchFinder::MatchResult &Result, const NodeType &Node);

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

  DeclScanner(StringRef Target, ArrayRef<std::string> Filename,
                    const ClassWrapperContext &Context);

  MatchHandler<RecordDecl, RecordDeclID> RecordDeclHandler;

};



// using NeedToWrapFunc = std::function<bool(const StringRef &)>;
// using RecordSymbolFunc = std::function<void(const SymbolRecordEntry&)>;
//
// extern std::unique_ptr<MatchFinder> newDeclScannerMatchFinderFactory(
//     const NeedToWrapFunc &NeedToWrap, const RecordSymbolFunc &RecordSymbol,
//     const std::shared_ptr<ExtendedODRHash::ODRHashCache> &TypeHashCache);

} // namespace clang::class_wrapper

#endif // DECLSCANNER_H

/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#ifndef DECLSCANNER_H
#define DECLSCANNER_H

#include "ClassWrapperContext.h"
// #include "ExtendedODRHash.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "llvm/ADT/ArrayRef.h"

namespace clang::class_wrapper {

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;


class RefEntry {
  friend class DeclEntry;
  std::string Name;
  Decl::Kind Kind;
  StorageClass Storage = SC_Extern;

  // points to the SameRange of DeclEntry, only set when UsedAsFunctionPtr
  tooling::Range Range;

  bool UsedAsFunctionPtr = false;
};


class DeclEntry {
  std::string Name;
  std::string FilePath; // source file's path relative to SourceRoot
  Decl::Kind Kind;
  StorageClass Storage;

  std::string Expansion;

  // If the Expansion is empty, the following Ranges points to the sources;
  // otherwise, the Ranges points to the Expansion.
  tooling::Range NameRange;
  tooling::Range InfRange;
  tooling::Range ImplRange;

  llvm::hash_code InfHash;
  llvm::hash_code ImplHash;

  bool IsDefinition;
  bool IsInline = false;
  SmallVector<RefEntry, 4> InfRefs;  // the symbols used in the declaration
  SmallVector<RefEntry, 4> ImplRefs; // the symbols used in the definition
};


constexpr const char TypedefDeclStr[] = "typedefDecl";
const auto TypedefDeclMatcher = traverse(TK_IgnoreUnlessSpelledInSource,
                                   typedefDecl().bind(TypedefDeclStr));
constexpr const char RecordDeclStr[] = "recordDecl";
//const auto RecordDeclMatcher = recordDecl().bind(RecordDeclStr);
const auto RecordDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, recordDecl().bind(RecordDeclStr));
constexpr const char EnumDeclStr[] = "enumDecl";
const auto EnumDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, enumDecl().bind(EnumDeclStr));
constexpr const char VarDeclStr[] = "varDecl";
const auto VarDeclMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, varDecl().bind(VarDeclStr));
constexpr const char FunctionDeclStr[] = "functionDecl";
const auto FunctionDeclMatcher = traverse(TK_IgnoreUnlessSpelledInSource,
                                    functionDecl().bind(FunctionDeclStr));
constexpr const char DeclStmtStr[] = "declStmt";
const auto DeclStmtMatcher =
    traverse(TK_IgnoreUnlessSpelledInSource, declStmt().bind(DeclStmtStr));
constexpr const char DeclRefExprStr[] = "declRefExpr";
const auto DeclRefExprMatcher = traverse(TK_IgnoreUnlessSpelledInSource,
                                   declRefExpr().bind(DeclRefExprStr));


using NeedToWrapFunc = std::function<bool(const StringRef &)>;
using RecordSymbolFunc = std::function<void(const SymbolRecordEntry&)>;

extern std::unique_ptr<MatchFinder> newDeclScannerMatchFinderFactory(
    const NeedToWrapFunc &NeedToWrap, const RecordSymbolFunc &RecordSymbol,
    const std::shared_ptr<ExtendedODRHash::ODRHashCache> &TypeHashCache);

extern void runDeclScanner(StringRef Target, StringRef Filename,
    const CompilationDatabase &Compilations,
    const ClassWrapperContext &Context);
} // namespace clang::class_wrapper

#endif // DECLSCANNER_H

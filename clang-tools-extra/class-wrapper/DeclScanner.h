/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#ifndef DECLSCANNER_H
#define DECLSCANNER_H

#include "ClassWrapperContext.h"
#include "ExtendedODRHash.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "llvm/ADT/ArrayRef.h"

namespace clang::class_wrapper {

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

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

extern void runDeclScanner(const CompilationDatabase &Compilations,
                           const ArrayRef<std::string> SourcePaths,
                           ClassWrapperContext &Context);

} // namespace clang::class_wrapper

#endif // DECLSCANNER_H

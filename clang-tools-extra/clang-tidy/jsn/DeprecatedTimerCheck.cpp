//===--- UpgradeGoogletestCaseCheck.cpp - clang-tidy ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


#include "DeprecatedTimerCheck.h"
//#include "../utils/TransformerClangTidyCheck.h"
//#include "clang/Tooling/Transformer/Stencil.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/FormatVariadic.h"
#include <functional>

using namespace clang::ast_matchers;
//using clang::transformer::cat;
//using clang::transformer::node;

namespace clang::tidy::jsn {

namespace {
StringRef getExprSourceText(const Expr *E,
                            const MatchFinder::MatchResult &Result) {
  return Lexer::getSourceText(
      CharSourceRange::getTokenRange(E->getBeginLoc(), E->getEndLoc()),
      *Result.SourceManager, Result.Context->getLangOpts(), nullptr);
}
} // namespace

void DeprecatedTimerCheck::replaceCreate(const MatchFinder::MatchResult &Result, const CallExpr *C) {
  const Expr * ArgOpt = C->getArg(2);
  Expr::EvalResult EvalResult;
  if (ArgOpt->isValueDependent() || !ArgOpt->EvaluateAsInt(EvalResult, *Result.Context)) {
    diag(ArgOpt->getExprLoc(), "invalid opt value", DiagnosticIDs::Error);
    return;
  }
  const auto MacroTmrPeriodic = (1 << 0);
  const auto MacroTmrTrigger = (1 << 1);
  const auto & OptValue = EvalResult.Val.getInt();

  if (OptValue != MacroTmrPeriodic && OptValue != MacroTmrTrigger){
    diag(ArgOpt->getExprLoc(), "invalid opt value", DiagnosticIDs::Error);
    return;
  }

  DiagnosticBuilder Diag = diag(C->getExprLoc(),
                                "replace with Nos_CreateTimer");

  StringRef ArgFirst = getExprSourceText(C->getArg(0), Result);
  StringRef ArgInterval = getExprSourceText(C->getArg(1), Result);
  StringRef ArgFn = getExprSourceText(C->getArg(3), Result);
  StringRef ArgArg = getExprSourceText(C->getArg(4), Result);
  StringRef ArgName = getExprSourceText(C->getArg(5), Result);

  std::string Replacement;

  if (OptValue == MacroTmrPeriodic){
    Replacement = llvm::formatv("Nos_CreateTimer({0}, {1}, {2}, {3}, {4})", ArgFirst,
                                ArgInterval, ArgFn, ArgArg, ArgName);
  }else{
    Replacement = llvm::formatv("Nos_CreateTimer({0}, 0, {1}, {2}, {3})", ArgFirst,
                                ArgFn, ArgArg, ArgName);
  }

  Diag << FixItHint::CreateReplacement(C->getSourceRange(), Replacement);
} // namespace

void DeprecatedTimerCheck::replaceDelete(const MatchFinder::MatchResult &Result, const CallExpr *C) {}
void DeprecatedTimerCheck::replaceStart(const MatchFinder::MatchResult &Result, const CallExpr *C) {}
void DeprecatedTimerCheck::replaceStop(const MatchFinder::MatchResult &Result, const CallExpr *C) {}
void DeprecatedTimerCheck::replaceModify(const MatchFinder::MatchResult &Result, const CallExpr *C) {}
void DeprecatedTimerCheck::replaceActivate(const MatchFinder::MatchResult &Result, const CallExpr *C) {}
void DeprecatedTimerCheck::replaceRemain(const MatchFinder::MatchResult &Result, const CallExpr *C) {}

const std::pair<StringRef, std::function<void(
                               DeprecatedTimerCheck &,
                               const ast_matchers::MatchFinder::MatchResult &,
                               const CallExpr *)>>
    DeprecatedTimerCheck::MatcherPatterns[] = {
        {"timer_create", &DeprecatedTimerCheck::replaceCreate},
        {"timer_create", &DeprecatedTimerCheck::replaceDelete},
        {"timer_start", &DeprecatedTimerCheck::replaceStart},
        {"timer_stop", &DeprecatedTimerCheck::replaceStop},
        {"timer_modify", &DeprecatedTimerCheck::replaceModify},
        {"timer_activate", &DeprecatedTimerCheck::replaceActivate},
        {"timer_remain", &DeprecatedTimerCheck::replaceRemain},
};

void DeprecatedTimerCheck::registerMatchers(MatchFinder *Finder) {
  for (const auto &[Name, _] : MatcherPatterns) {
    Finder->addMatcher(callExpr(callee(functionDecl(hasName(Name)))).bind(Name),
                       this);
  }
}

void DeprecatedTimerCheck::check(const MatchFinder::MatchResult &Result){
  for (const auto &[Name, Callback] : MatcherPatterns){
    if (const auto * MatchedCall = Result.Nodes.getNodeAs<CallExpr>(Name)){
      Callback(*this, Result, MatchedCall);
      return;
    }
  }
}

} // namespace clang::tidy::jsn

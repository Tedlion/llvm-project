//===--- UpgradeGoogletestCaseCheck.cpp - clang-tidy ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "DeprecatedTimerCheck.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/FormatVariadic.h"
#include <functional>

using namespace clang::ast_matchers;

namespace clang::tidy::jsn {

namespace {
StringRef getExprSourceText(const Expr *E,
                            const MatchFinder::MatchResult &Result) {
  return Lexer::getSourceText(
      CharSourceRange::getTokenRange(E->getBeginLoc(), E->getEndLoc()),
      *Result.SourceManager, Result.Context->getLangOpts(), nullptr);
}

enum TimerType {
  Invalid,
  Periodic,
  Trigger,
};

TimerType checkTimerType(const Expr *Opt, ASTContext &Context) {
  Expr::EvalResult EvalResult;
  if (Opt->isValueDependent() || !Opt->EvaluateAsInt(EvalResult, Context)) {
    return Invalid;
  }

  const auto MacroTmrPeriodic = (1 << 0);
  const auto MacroTmrTrigger = (1 << 1);
  const auto &OptValue = EvalResult.Val.getInt();

  if (OptValue == MacroTmrPeriodic) {
    return Periodic;
  }
  if (OptValue == MacroTmrTrigger) {
    return Trigger;
  }
  return Invalid;
}

} // namespace

void DeprecatedTimerCheck::replaceCreate(const MatchFinder::MatchResult &Result,
                                         const CallExpr *C) {
  const Expr *ArgOpt = C->getArg(2);
  TimerType TmrType = checkTimerType(ArgOpt, *Result.Context);
  if (TmrType == Invalid) {
    diag(ArgOpt->getExprLoc(), "invalid opt value", DiagnosticIDs::Error);
  }

  DiagnosticBuilder Diag =
      diag(C->getExprLoc(), "replace with Nos_CreateTimer");

  StringRef ArgFirst = getExprSourceText(C->getArg(0), Result);
  StringRef ArgInterval = getExprSourceText(C->getArg(1), Result);
  StringRef ArgFn = getExprSourceText(C->getArg(3), Result);
  StringRef ArgArg = getExprSourceText(C->getArg(4), Result);
  StringRef ArgName = getExprSourceText(C->getArg(5), Result);

  std::string Replacement;

  if (TmrType == Periodic) {
    Replacement = llvm::formatv("Nos_CreateTimer({0}, {1}, {2}, {3}, {4})",
                                ArgFirst, ArgInterval, ArgFn, ArgArg, ArgName);
  } else {
    Replacement = llvm::formatv("Nos_CreateTimer({0}, 0, {1}, {2}, {3})",
                                ArgFirst, ArgFn, ArgArg, ArgName);
  }

  Diag << FixItHint::CreateReplacement(C->getSourceRange(), Replacement);
} // namespace

void DeprecatedTimerCheck::replaceDelete(const MatchFinder::MatchResult &Result,
                                         const CallExpr *C) {
  StringRef ArgTmr = getExprSourceText(C->getArg(0), Result);
  DiagnosticBuilder Diag =
      diag(C->getExprLoc(), "replace with Nos_DeleteTimer");

  Diag << FixItHint::CreateReplacement(
      C->getSourceRange(), llvm::formatv("Nos_DeleteTimer({0})", ArgTmr).str());
}

void DeprecatedTimerCheck::replaceStart(const MatchFinder::MatchResult &Result,
                                        const CallExpr *C) {
  StringRef ArgTmr = getExprSourceText(C->getArg(0), Result);
  DiagnosticBuilder Diag =
      diag(C->getExprLoc(), "replace with Nos_StartTimer\n");

  // FIXME: Nos_DeleteTimer has no return value.
  //  Although the old return value is always zero, all usages taken the return
  //  value have to be manually checked.
  //  A totally auto refactor seems to be challenging.

  Diag << FixItHint::CreateReplacement(
      C->getSourceRange(), llvm::formatv("Nos_StartTimer({0})", ArgTmr).str());
}

void DeprecatedTimerCheck::replaceStop(const MatchFinder::MatchResult &Result,
                                       const CallExpr *C) {
  StringRef ArgTmr = getExprSourceText(C->getArg(0), Result);
  DiagnosticBuilder Diag =
      diag(C->getExprLoc(), "replace with Nos_StopTimer\n");

  Diag << FixItHint::CreateReplacement(
      C->getSourceRange(), llvm::formatv("Nos_StopTimer({0})", ArgTmr).str());
}

void DeprecatedTimerCheck::replaceModify(const MatchFinder::MatchResult &Result,
                                         const CallExpr *C) {
  const Expr *ArgOpt = C->getArg(3);
  TimerType TmrType = checkTimerType(ArgOpt, *Result.Context);
  if (TmrType == Invalid) {
    diag(ArgOpt->getExprLoc(), "invalid opt value", DiagnosticIDs::Error);
    return;
  }

  DiagnosticBuilder Diag =
      diag(C->getExprLoc(), "replace with Nos_ModifyTimer");

  StringRef ArgTmr = getExprSourceText(C->getArg(0), Result);
  StringRef ArgFirst = getExprSourceText(C->getArg(1), Result);
  StringRef ArgInterval = getExprSourceText(C->getArg(2), Result);
  StringRef ArgFn = getExprSourceText(C->getArg(4), Result);
  StringRef ArgArg = getExprSourceText(C->getArg(5), Result);

  std::string Replacement;

  if (TmrType == Periodic) {
    Replacement = llvm::formatv("Nos_ModifyTimer({0}, {1}, {2}, {3}, {4})",
                                ArgTmr, ArgFirst, ArgInterval, ArgFn, ArgArg);
  } else {
    Replacement = llvm::formatv("Nos_ModifyTimer({0}, {1}, 0, {2}, {3})",
                                ArgTmr, ArgFirst, ArgFn, ArgArg);
  }

  Diag << FixItHint::CreateReplacement(C->getSourceRange(), Replacement);
}

void DeprecatedTimerCheck::replaceActivate(
    const MatchFinder::MatchResult &Result, const CallExpr *C) {
  const Expr *ArgOpt = C->getArg(3);
  TimerType TmrType = checkTimerType(ArgOpt, *Result.Context);
  if (TmrType == Invalid) {
    diag(ArgOpt->getExprLoc(), "invalid opt value", DiagnosticIDs::Error);
    return;
  }

  DiagnosticBuilder Diag =
      diag(C->getExprLoc(), "replace with Nos_ActivateTimer");

  StringRef ArgTmr = getExprSourceText(C->getArg(0), Result);
  StringRef ArgFirst = getExprSourceText(C->getArg(1), Result);
  StringRef ArgInterval = getExprSourceText(C->getArg(2), Result);

  std::string Replacement;

  if (TmrType == Periodic) {
    Replacement = llvm::formatv("Nos_ActivateTimer({0}, {1} {2})", ArgTmr,
                                ArgFirst, ArgInterval);
  } else {
    Replacement =
        llvm::formatv("Nos_ActivateTimer({0}, {1}, 0)", ArgTmr, ArgFirst);
  }

  Diag << FixItHint::CreateReplacement(C->getSourceRange(), Replacement);
}

void DeprecatedTimerCheck::replaceRemain(const MatchFinder::MatchResult &Result,
                                         const CallExpr *C) {
  StringRef ArgTmr = getExprSourceText(C->getArg(0), Result);
  DiagnosticBuilder Diag =
      diag(C->getExprLoc(), "replace with Nos_TimerRemain\n");

  Diag << FixItHint::CreateReplacement(
      C->getSourceRange(), llvm::formatv("Nos_TimerRemain({0})", ArgTmr).str());
}

const std::pair<StringRef, std::function<void(
                               DeprecatedTimerCheck &,
                               const ast_matchers::MatchFinder::MatchResult &,
                               const CallExpr *)>>
    DeprecatedTimerCheck::MatcherPatterns[] = {
        {"timer_create", &DeprecatedTimerCheck::replaceCreate},
        {"timer_delete", &DeprecatedTimerCheck::replaceDelete},
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

void DeprecatedTimerCheck::check(const MatchFinder::MatchResult &Result) {
  for (const auto &[Name, Callback] : MatcherPatterns) {
    if (const auto *MatchedCall = Result.Nodes.getNodeAs<CallExpr>(Name)) {
      Callback(*this, Result, MatchedCall);
      return;
    }
  }
}

} // namespace clang::tidy::jsn

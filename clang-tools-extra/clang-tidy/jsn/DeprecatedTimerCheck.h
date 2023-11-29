//===--- OverloadedUnaryAndCheck.h - clang-tidy -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_JSN_DEPRECATEDTIMERCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_JSN_DEPRECATEDTIMERCHECK_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::jsn {

class DeprecatedTimerCheck : public ClangTidyCheck{
public:
  DeprecatedTimerCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;

private:
  void replaceCreate(const ast_matchers::MatchFinder::MatchResult &Result, const CallExpr *C);
  void replaceDelete(const ast_matchers::MatchFinder::MatchResult &Result, const CallExpr *C);
  void replaceStart(const ast_matchers::MatchFinder::MatchResult &Result, const CallExpr *C);
  void replaceStop(const ast_matchers::MatchFinder::MatchResult &Result, const CallExpr *C);
  void replaceModify(const ast_matchers::MatchFinder::MatchResult &Result, const CallExpr *C);
  void replaceActivate(const ast_matchers::MatchFinder::MatchResult &Result, const CallExpr *C);
  void replaceRemain(const ast_matchers::MatchFinder::MatchResult &Result, const CallExpr *C);

  static const std::pair<
      StringRef,
      std::function<void(DeprecatedTimerCheck &,
                         const ast_matchers::MatchFinder::MatchResult &,
                         const CallExpr *)>>
      MatcherPatterns[];
};


} // namespace clang::tidy::jsn

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_JSN_DEPRECATEDTIMERCHECK_H

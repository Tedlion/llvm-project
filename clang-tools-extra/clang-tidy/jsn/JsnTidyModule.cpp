//===--- JsnTidyModule.cpp - clang-tidy -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
#include "DeprecatedTimerCheck.h"

//using namespace clang::ast_matchers;

namespace clang::tidy {
namespace jsn {

class JsnModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<DeprecatedTimerCheck>("jsn-deprecated-timer");
  }
};



// Register the GoogleTidyModule using this statically initialized variable.
static ClangTidyModuleRegistry::Add<JsnModule> X("Jsn-module",
                                                 "Adds Jsn lint checks.");

}  // namespace jsn

// This anchor is used to force the linker to link in the generated object file
// and thus register the GoogleModule.
volatile int JsnModuleAnchorSource = 0;

} // namespace clang::tidy

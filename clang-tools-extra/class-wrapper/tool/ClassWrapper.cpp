//===-- ClassWrapper.cpp - a tool to wrap code of with one single class ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/WithColor.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory ClangQueryCategory("class wrapper options");


int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::Expected<CommonOptionsParser> OptionsParser =
      CommonOptionsParser::create(argc, argv, ClangQueryCategory,
                                  llvm::cl::OneOrMore);

  if (!OptionsParser) {
    llvm::WithColor::error() << llvm::toString(OptionsParser.takeError());
    return 1;
  }

  return 0;
}
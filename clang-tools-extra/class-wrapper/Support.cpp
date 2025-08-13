// ===-- Support.cpp - Support Utils Implementation  -----------------------===/
//
//  Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
//  See https://llvm.org/LICENSE.txt for license information.
//  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ===----------------------------------------------------------------------===/

#include "Support.h"
#include "llvm/Support/FileSystem.h"


namespace clang::class_wrapper {
bool needUpdate(StringRef TargetFile, ArrayRef<std::string> Dependencies) {
  using namespace llvm::sys::fs;
  file_status Target, Dependency;
  if (status(TargetFile, Target))
    return true;
  auto TargetModified = Target.getLastModificationTime();
  for (auto &DepFile : Dependencies) {
    if (status(DepFile, Dependency))
      continue;
    auto DepModified = Dependency.getLastModificationTime();
    if (DepModified > TargetModified)
      return true; // dependency is newer than target
  }
  return false;
}

}


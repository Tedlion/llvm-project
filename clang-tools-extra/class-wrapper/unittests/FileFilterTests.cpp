// ===-- FileFilterTests.cpp - File filter unit tests ----------------------===/
//
//  Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
//  See https://llvm.org/LICENSE.txt for license information.
//  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ===----------------------------------------------------------------------===/

#include "FileFilter.h"

#include "gtest/gtest.h"

namespace llvm {
namespace {
TEST(FileFilterTest, DefaultRuleMatchesSourceRootFiles) {
  std::vector<std::string> Rules;
  FileFilter Filter(Rules.begin(), Rules.end(), "src");

  EXPECT_TRUE(Filter.isMatched("src/foo.cpp"));
  EXPECT_TRUE(Filter.isMatched("src/nested/bar.cpp"));
  EXPECT_FALSE(Filter.isMatched("other/foo.cpp"));
}


TEST(FileFilterTest, StoredPatternsRemainValidAfterConstructor) {
  std::vector<std::string> Rules;
  for (int I = 0; I != 128; ++I)
    Rules.push_back(std::string("+generated_") + std::to_string(I) + "/**");
  Rules.push_back("+./keep.cpp");
  Rules.push_back("-generated_42/exclude.cpp");

  FileFilter Filter(Rules.begin(), Rules.end(), "src");

  EXPECT_TRUE(Filter.isMatched("src/keep.cpp"));
  EXPECT_TRUE(Filter.isMatched("src/generated_17/file.h"));
  EXPECT_FALSE(Filter.isMatched("src/generated_42/exclude.cpp"));
  EXPECT_FALSE(Filter.isMatched("src/other.cpp"));
}
} // namespace
} // namespace llvm


//===- class-wrapper/unittests/DeclScannerTests.cpp ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../unittests/ASTMatchers/ASTMatchersTest.h"
#include "DeclScanner.h"
#include "ExtendedODRHash.h"
#include "gtest/gtest.h"
#include <set>

using namespace clang;
using namespace clang::ast_matchers;

namespace clang::class_wrapper {

class DeclScannerTest : public ::testing::Test {
private:



  const NeedToWrapFunc NeedToWrap = [](StringRef) { return true; };
  const RecordSymbolFunc RecordSymbol = [this](StringRef Name, CharSourceRange Range,
                                           Decl::Kind Kind, StorageClass Storage,
                                           ExtendedODRHash::HashValue InfHash,
                                           ExtendedODRHash::HashValue ImplHash,
                                           bool IsInline, bool IsFuncPtr) {


  };

  std::unique_ptr<MatchFinder> ScannerFinder;


  std::set<ExtendedODRHash::HashValue> HashRecords;

  template <typename MatcherHandler, typename NodeType, auto bindName>
  class VerifyHashValue : public MatchFinder::MatchCallback {
  private:
    std::optional<ExtendedODRHash::HashValue> HashValue = std::nullopt;
    unsigned MatchedCount = 0;
    const unsigned ExpectedMatchedIndex;

  public:



    std::optional<ExtendedODRHash::HashValue> getHashValue() const {
      return HashValue;
    }

    void run(const MatchFinder::MatchResult &Result) override {
      if (MatchedCount == ExpectedMatchedIndex) {
        const auto *Node = Result.Nodes.getNodeAs<NodeType>(bindName);
        if (!Node) {
          ADD_FAILURE() << "Expected Matched";
          return;
        }
        ExtendedODRHash::ODRHashCache HashCache;
        ExtendedODRHash Hash(HashCache);
        Hash.AddRecordDecl(Node);
        HashValue = Hash.getHashValue();
      }
      ++MatchedCount;
    }

  };

  Match

public:
  /**
   * @brief
   * @param Code
   * @param AMatcher
   * @param ExpectedHashValue if ExpectedHashValue equals std::nullopt,
   * the hash value of the matched node is supposed to be unique,
   * i.e., not in HashRecords.
   * @Param HashValue if not nullptr, the hash value of the matched node will be
   * stored in it.
   * @param CompileArgs
   * @param MatchedIndex calculate the hash value of the MatchedIndex-th matched
   * node
   * @param Filename
   * @param VirtualMappedFiles
   * @return
   */
  template <typename T>
  testing::AssertionResult matchAndCheckHashValue(
      const Twine &Code, const T &AMatcher,
      std::optional<ExtendedODRHash::HashValue> ExpectedHashValue,
      ExtendedODRHash::HashValue *HashValue = nullptr,
      ArrayRef<std::string> CompileArgs = {}, unsigned MatchedIndex = 0,
      StringRef Filename = "input.c",
      const FileContentMappings &VirtualMappedFiles = FileContentMappings()) {

Finder Finder;

    if (ExpectedHashValue.) {
      HashRecords.insert(*ExpectedHashValue);
    }
    return matchAndVerifyResultConditionally(
        Code, AMatcher, std::move(FindResultVerifier), true, "input.c");
  }
};

class VerifyHashUnique : public BoundNodesCallback {
public:
  VerifyHashUnique(std::vector<ExtendedODRHash::HashValue> &HashRecords,
                   size_t &RecordIndex)
      : HashRecords(HashRecords), RecordIndex(RecordIndex) {}

  bool run(const BoundNodes *Nodes) override {
    ADD_FAILURE() << "Should not be called";
    return false;
  }

  bool run(const BoundNodes *Nodes, ASTContext *Context) override {

  }

private:
  std::vector<ExtendedODRHash::HashValue> & HashRecords;
  size_t & RecordIndex;

};


TEST_P(DeclScannerTest, TestSimpleStructDefine){
  StringRef Input = R"c(
struct S {
  int a;
  int b;
};
)c";


  ExtendedODRHash::HashValue HashValue;
  ASSERT_TRUE(matchAndCheckHashValue(Input, recordDecl(hasName("S")),
                                     std::nullopt, &HashValue));
  ASSERT_TRUE(HashValue.Valid);
  ASSERT_TRUE(HashRecords.insert(HashValue).second);
}




} // namespace clang::class_wrapper
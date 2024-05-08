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
#include <expected>
#include <set>

using namespace clang;
using namespace clang::ast_matchers;

namespace clang::class_wrapper {

class DeclScannerTest : public ::testing::Test {
private:
  std::vector<SymbolRecordEntry> MatchedSymbols;
  std::multimap<ExtendedODRHash::HashValue,
                std::vector<SymbolRecordEntry>::const_pointer>
      HashRecords;

  const NeedToWrapFunc NeedToWrap = [](StringRef) { return true; };

  const RecordSymbolFunc RecordSymbol = [this](const SymbolRecordEntry &Entry) {
    MatchedSymbols.push_back(Entry);
    if (Entry.InfHash.Valid)
      HashRecords.insert({Entry.InfHash, &MatchedSymbols.back()});
    if (Entry.ImplHash.Valid)
      HashRecords.insert({Entry.ImplHash, &MatchedSymbols.back()});
  };

  bool Found = false;

protected:
  using ScanResults = std::expected<std::span<const SymbolRecordEntry>, testing::AssertionResult>;

  ScanResults
  scanOnCode(StringRef Code, StringRef Filename = "input.c",
             std::shared_ptr<ExtendedODRHash::ODRHashCache> HashCache =
                 std::make_shared<ExtendedODRHash::ODRHashCache>()) {
    auto MatchedResultEnd = MatchedSymbols.end();

    std::unique_ptr<MatchFinder> ScannerFinder =
        newDeclScannerMatchFinderFactory(NeedToWrap, RecordSymbol, HashCache);

    std::unique_ptr<FrontendActionFactory> Factory(
        newFrontendActionFactory(ScannerFinder.get()));
    std::vector<std::string> Args = {"-target", "i386-unknown-unknown"};
    if (!runToolOnCodeWithArgs(Factory->create(), Code, Args, Filename)) {
      return std::unexpected(testing::AssertionFailure()
                             << "Parsing error in \"" << Code << "\"");
    }

    return std::span<const SymbolRecordEntry>(MatchedResultEnd, MatchedSymbols.end());
  }

  static const SymbolRecordEntry * findInScanResults(const ScanResults &Results,
                                                    StringRef Name,
                                                    unsigned Index = 0) {
    if (!Results)
      return nullptr;
    for (const SymbolRecordEntry &Entry : Results.value()) {
      unsigned MatchedIndex = 0;
      if (Entry.Name == Name) {
        MatchedIndex++;
        if (MatchedIndex == Index) {
          return &Entry;
        }
      }
    }
    return nullptr;
  }

};


TEST_F(DeclScannerTest, TestSimpleStructDefine){
  StringRef Input = R"c(
struct S {
  int a;
  int b;
};
)c";

  ScanResults Results = scanOnCode(Input);
  const SymbolRecordEntry *Entry = findInScanResults(Results, "S");
  ASSERT_TRUE(Entry);

  EXPECT_EQ(Entry->Kind, Decl::Kind::CXXRecord);

//  ExtendedODRHash::HashValue HashValue;
//  ASSERT_TRUE(matchAndCheckHashValue(Input, recordDecl(hasName("S")),
//                                     std::nullopt, &HashValue));
//  ASSERT_TRUE(HashValue.Valid);
//  ASSERT_TRUE(HashRecords.insert(HashValue).second);
}




} // namespace clang::class_wrapper
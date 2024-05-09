//===- class-wrapper/unittests/DeclScannerTests.cpp
//------------------------===//
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
  using ScanResults = std::expected<std::span<const SymbolRecordEntry>,
                                    testing::AssertionResult>;

  ScanResults
  scanOnCode(StringRef Code, StringRef Filename = "input.c",
             std::shared_ptr<ExtendedODRHash::ODRHashCache> HashCache =
                 std::make_shared<ExtendedODRHash::ODRHashCache>()) {
    size_t MatchedCount = MatchedSymbols.size();

    std::unique_ptr<MatchFinder> ScannerFinder =
        newDeclScannerMatchFinderFactory(NeedToWrap, RecordSymbol, HashCache);

    std::unique_ptr<FrontendActionFactory> Factory(
        newFrontendActionFactory(ScannerFinder.get()));
    std::vector<std::string> Args = {"-target", "i386-unknown-unknown"};
    if (!runToolOnCodeWithArgs(Factory->create(), Code, Args, Filename)) {
      return std::unexpected(testing::AssertionFailure()
                             << "Parsing error in \"" << Code << "\"");
    }

    return std::span<const SymbolRecordEntry>(
        MatchedSymbols.begin() + MatchedCount, MatchedSymbols.end());
  }

  static const SymbolRecordEntry *findInScanResults(const ScanResults &Results,
                                                    StringRef Name,
                                                    unsigned Index = 0) {
    if (!Results)
      return nullptr;
    for (const SymbolRecordEntry &Entry : Results.value()) {
      unsigned MatchedIndex = 0;
      if (Entry.Name == Name) {
        if (MatchedIndex == Index) {
          return &Entry;
        }
        MatchedIndex++;
      }
    }
    return nullptr;
  }

  static testing::AssertionResult
  verifyRangeMatched(StringRef Source, Range CharRange, StringRef Expected) {
    if (CharRange.getOffset() + CharRange.getLength() > Source.size())
      return testing::AssertionFailure()
             << "Invalid range: " << CharRange.getOffset() << " + "
             << CharRange.getLength() << " > " << Source.size();
    StringRef MatchedText =
        Source.substr(CharRange.getOffset(), CharRange.getLength());
    if (MatchedText != Expected)
      return testing::AssertionFailure()
             << "got unexpected matched text \"" << MatchedText << "\"";
    return testing::AssertionSuccess();
  }

  static testing::AssertionResult verifyRangeMatched(StringRef Source,
                                                     Range CharRange,
                                                     StringRef ExpectedBegin,
                                                     StringRef ExpectedEnd) {
    if (CharRange.getOffset() + CharRange.getLength() > Source.size())
      return testing::AssertionFailure()
             << "Invalid range: " << CharRange.getOffset() << " + "
             << CharRange.getLength() << " > " << Source.size();
    if (CharRange.getLength() < ExpectedBegin.size() ||
        CharRange.getLength() < ExpectedEnd.size()) {
      return testing::AssertionFailure() << "Matched text too short";
    }

    StringRef MatchedText =
        Source.substr(CharRange.getOffset(), CharRange.getLength());

    if (!MatchedText.starts_with(ExpectedBegin) ||
        !MatchedText.ends_with(ExpectedEnd)) {
      return testing::AssertionFailure()
             << "got unexpected matched text \"" << MatchedText << "\"";
    }
    return testing::AssertionSuccess();
  }

  size_t getHashRecordCount(ExtendedODRHash::HashValue Hash) const {
    return HashRecords.count(Hash);
  }
};

TEST_F(DeclScannerTest, TestSimpleStructDefine) {
  StringRef Input = R"c(
struct S {
  int a;
  int b;
};
)c";

  ScanResults Results =
      scanOnCode(Input, "DeclScannerTest/testSimpleStructDefine.c");
  const SymbolRecordEntry *Entry = findInScanResults(Results, "S");
  ASSERT_TRUE(Entry);
  EXPECT_STREQ(Entry->FilePath.data(),
               "DeclScannerTest/testSimpleStructDefine.c");
  EXPECT_TRUE(verifyRangeMatched(Input, Entry->CharRange,
                                 "struct S {\n"
                                 "  int a;\n"
                                 "  int b;\n"
                                 "};\n"));
  EXPECT_TRUE(verifyRangeMatched(Input, Entry->CharRange, "struct S", "};\n"));
  EXPECT_EQ(Entry->Kind, Decl::Kind::Record);
  EXPECT_EQ(Entry->Storage, StorageClass::SC_Extern);
  EXPECT_FALSE(Entry->InfHash.Valid);
  EXPECT_TRUE(Entry->ImplHash.Valid);
  EXPECT_TRUE(Entry->ImplHash.Completed);
  EXPECT_FALSE(Entry->IsInline);
  EXPECT_FALSE(Entry->IsFuncPtr);
  EXPECT_EQ(getHashRecordCount(Entry->ImplHash), 1);
}

TEST_F(DeclScannerTest, TestStructDefineSameHash) {
  StringRef Input1 = R"c(
struct S {
  int a;
  int b;
};
)c";

  StringRef Input2 = R"c(
struct S{
int a;
//some comment here
  int b;};
)c";

  ScanResults Results1 = scanOnCode(Input1, "input1.c");
  const SymbolRecordEntry Entry1 = *findInScanResults(Results1, "S");
  ScanResults Results2 = scanOnCode(Input2, "input2.c");
  const SymbolRecordEntry Entry2 = *findInScanResults(Results2, "S");
  EXPECT_EQ(Entry1.ImplHash, Entry2.ImplHash);
}

} // namespace clang::class_wrapper
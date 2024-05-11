//===- class-wrapper/unittests/DeclScannerTests.cpp
//------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "GtestSupport.h"
#include "../Support.h"
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
  using ScanResults = std::expected<
      std::pair<size_t /*begin_index*/, size_t /*end_index(not contained)*/>,
      testing::AssertionResult>;

  /**
   * @brief
   * @param Code
   * @param Filename
   * @param CompileArgs
   * @param HashCache Pass an outer HashCache if you want to use a same cache
   * for multiple scans
   * @return std::pair of the begin index and the end index of new matched symbols, testing::AssertionFailure if code parsing fail.
   */
  ScanResults
  scanOnCode(StringRef Code, StringRef Filename = "input.c",
             std::vector<std::string> CompileArgs = {},
             std::shared_ptr<ExtendedODRHash::ODRHashCache> HashCache =
                 std::make_shared<ExtendedODRHash::ODRHashCache>()) {
    size_t MatchedCount = MatchedSymbols.size();

    std::unique_ptr<MatchFinder> ScannerFinder =
        newDeclScannerMatchFinderFactory(NeedToWrap, RecordSymbol, HashCache);

    std::unique_ptr<FrontendActionFactory> Factory(
        newFrontendActionFactory(ScannerFinder.get()));
    if (llvm::find(CompileArgs, "-target") == CompileArgs.end()) {
      CompileArgs.push_back("-target");
      CompileArgs.push_back("i386-unknown-unknown");
    }
    if (!runToolOnCodeWithArgs(Factory->create(), Code, CompileArgs,
                               Filename)) {
      return std::unexpected(testing::AssertionFailure()
                             << "Parsing error in \"" << Code << "\"");
    }

    return std::make_pair(MatchedCount, MatchedSymbols.size());
  }

  std::expected<SymbolRecordEntry, testing::AssertionResult>
  findInScanResults(const ScanResults &Results, StringRef Name,
                    unsigned Index = 0) {
    if (!Results)
      return std::unexpected(testing::AssertionFailure()
                                    << "ScanResults not valid");
    for (size_t I = Results->first, End = Results->second; I < End;
         ++I) {
      unsigned MatchedIndex = 0;
      if (MatchedSymbols[I].Name == Name) {
        if (MatchedIndex == Index) {
          return MatchedSymbols[I];
        }
        MatchedIndex++;
      }
    }
    return std::unexpected(testing::AssertionFailure()
                           <<std::format("{} not found", Name));
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


StringRef SimpleStructS = R"c(
struct S {
  int a;
  int b;
};
)c";


StringRef SimpleStructSWithMacro = R"c(
struct S{
int a;
#if MACRO
char x;
#endif
  int b;};
)c";


TEST_F(DeclScannerTest, TestSimpleStructDefine) {
  ScanResults Results =
      scanOnCode(SimpleStructS, "DeclScannerTest/testSimpleStructDefine.c");
  auto EntryOrFail = findInScanResults(Results, "S");
  ASSERT_TRUE(EntryOrFail);
  const SymbolRecordEntry & Entry = EntryOrFail.value();
  EXPECT_STREQ(Entry.FilePath.data(),
               "DeclScannerTest/testSimpleStructDefine.c");
  EXPECT_TRUE(verifyRangeMatched(SimpleStructS, Entry.CharRange,
                                 "struct S {\n"
                                 "  int a;\n"
                                 "  int b;\n"
                                 "};\n"));
  EXPECT_TRUE(verifyRangeMatched(SimpleStructS, Entry.CharRange, "struct S", "};\n"));
  EXPECT_EQ(Entry.Kind, Decl::Kind::Record);
  EXPECT_EQ(Entry.Storage, StorageClass::SC_Extern);
  EXPECT_FALSE(Entry.InfHash.Valid);
  EXPECT_TRUE(Entry.ImplHash.Valid);
  EXPECT_TRUE(Entry.ImplHash.Completed);
  EXPECT_FALSE(Entry.IsInline);
  EXPECT_FALSE(Entry.IsFuncPtr);
  EXPECT_EQ(getHashRecordCount(Entry.ImplHash), 1);
}

TEST_F(DeclScannerTest, TestStructDefineSameHash) {
  auto EntryOrFail1 =
      findInScanResults(scanOnCode(SimpleStructS), "S");
  ASSERT_TRUE(EntryOrFail1);
  auto EntryOrFail2 =
      findInScanResults(scanOnCode(SimpleStructSWithMacro, "input2.c",
                                   {"-DMACRO1"}), "S");
  ASSERT_TRUE(EntryOrFail2);
  EXPECT_EQ(EntryOrFail1->ImplHash, EntryOrFail2->ImplHash);
}

TEST_F(DeclScannerTest, TEMP1){
  auto EntryOrFail1 =
      findInScanResults(scanOnCode(SimpleStructS, "input1.c"), "T");
        ASSERT_TRUE(EntryOrFail1);
}


} // namespace clang::class_wrapper
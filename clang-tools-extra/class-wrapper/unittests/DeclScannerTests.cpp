//===- DeclScannerTests.cpp -----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#include "GtestSupport.h"

#include "../Support.h"
#include "../unittests/ASTMatchers/ASTMatchersTest.h"
#include "DeclScanner.h"
#include "gtest/gtest.h"

#include <expected>

using namespace clang;
using namespace clang::ast_matchers;

namespace clang::class_wrapper {
class MatcherTest : public ::testing::Test, SourceFileCallbacks {
  template <std::derived_from<Decl> DeclNode, const char * BindID>
  class MatcherCallback : public MatchFinder::MatchCallback {
    MatcherTest &Test;
  public:
    MatcherCallback(MatcherTest &Test) : Test(Test) {}
    void run(const MatchFinder::MatchResult &Result) override {
      const auto *Node = Result.Nodes.getNodeAs<DeclNode>(BindID);
      if (!Node) {
        return;
      }

      if (auto Entry = getDeclEntry(Result, *Node, *Test.CI)) {
        Test.DeclEntries.push_back(*Entry);
      }
    }
  };

  MatchFinder Finder;
  std::vector<DeclEntry> DeclEntries;
  std::string ErrorMessage;
  MatcherCallback<RecordDecl, DeclScanner::RecordDeclID> RecordDeclHandler;
  const CompilerInstance * CI = nullptr;

  bool handleBeginSource(CompilerInstance &CI) override {
    this->CI = &CI;
    return true;
  }

protected:
  MatcherTest() : RecordDeclHandler(*this) {
  }

  template <typename T>
  void EnableMatcher();


  bool scanOnCode(StringRef Code, StringRef Filename = "input.c",
                  std::vector<std::string> CompileArgs = {}) {

    std::unique_ptr<FrontendActionFactory> Factory(
        newFrontendActionFactory(&Finder, this));
    if (llvm::find(CompileArgs, "-target") == CompileArgs.end()) {
      CompileArgs.push_back("-target");
      CompileArgs.push_back("i386-unknown-unknown");
    }
    CompileArgs.push_back("-fparse-all-comments");

    if (!runToolOnCodeWithArgs(Factory->create(),
                               Code, CompileArgs, Filename)) {
      ErrorMessage = std::format("Parsing error in \"{}\"", Code);
      return false;
    }
    return true;
  }

  [[nodiscard]] const std::vector<DeclEntry> &getResult() const {
    return DeclEntries;
  }

  [[nodiscard]] const std::string &getError() const {
    return ErrorMessage;
  }

};


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
           << "got unexpected matched text:\n \"" << MatchedText << "\"";
  return testing::AssertionSuccess();
}


static testing::AssertionResult
verifyRangeMatched(StringRef Source, Range CharRange,
                   StringRef ExpectedBegin, StringRef ExpectedEnd) {
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
           << "got unexpected matched text:\n \"" << MatchedText << "\"";
      }
  return testing::AssertionSuccess();
}


static hash_code getHashForStringList(ArrayRef<StringRef> Strings) {
  hash_code Hash = 0;
  for (const auto &S : Strings) {
    Hash = hash_combine(Hash, S);
  }
  return Hash;
}


template <>
void MatcherTest::EnableMatcher<RecordDecl>() {
  Finder.addMatcher(
      DeclScanner::RecordDeclMatcher, &RecordDeclHandler);
}

static StringRef SimpleStructS = R"c(
struct S {
  int a;
  int b;
};
)c";

TEST_F(MatcherTest, SimpleStruct) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(SimpleStructS, "a.c"));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.FilePath, "a.c");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);
  EXPECT_EQ(D.Storage, StorageClass::SC_None);

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_TRUE(verifyRangeMatched(SimpleStructS, D.NameRange, "S"));
  EXPECT_EQ(D.InfRange, Range(0, 0));
  EXPECT_TRUE(verifyRangeMatched(SimpleStructS, D.FullRange,
                                 SimpleStructS.ltrim()));

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "S", "{", "int", "a", ";", "int", "b", ";", "}", ";"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.isAnonymous);
  EXPECT_FALSE(D.isUnion);
}

static StringRef SimpleStructDecl = R"c(
struct S;
)c";


TEST_F(MatcherTest, SimpleStructDecl) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(SimpleStructDecl, "a.c"));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.FilePath, "a.c");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);
  EXPECT_EQ(D.Storage, StorageClass::SC_None);

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_TRUE(verifyRangeMatched(SimpleStructDecl, D.NameRange, "S"));
  EXPECT_EQ(D.InfRange, Range(0, 0));
  EXPECT_TRUE(verifyRangeMatched(SimpleStructDecl, D.FullRange,
                                 SimpleStructDecl.ltrim()));

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, hash_code(0));

  EXPECT_FALSE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.isAnonymous);
}


static StringRef StructWithComments = R"c(
// Comment 1
struct S {
  int a; // Comment 2
  int b;
}; // Comment3
)c";


TEST_F(MatcherTest, StructWithComments) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(StructWithComments));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);
  EXPECT_EQ(D.Storage, StorageClass::SC_None);

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_TRUE(verifyRangeMatched(StructWithComments, D.NameRange, "S"));
  EXPECT_EQ(D.InfRange, Range(0, 0));
  EXPECT_TRUE(verifyRangeMatched(StructWithComments, D.FullRange,
                                 "// Comment 1", "// Comment3\n"));

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "S", "{", "int", "a", ";", "int", "b", ";", "}", ";"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.isAnonymous);
}


StringRef SimpleStructSWithMacro = R"c(
struct S{
int a;
#if MACRO
char x;
#endif
  int b;};
)c";


TEST_F(MatcherTest, StructWithMacro) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(SimpleStructSWithMacro));
  ASSERT_TRUE(scanOnCode(SimpleStructSWithMacro, "b.c", {"-DMACRO"}));
  ASSERT_TRUE(scanOnCode(SimpleStructSWithMacro, "c.c", {"-DMACRO2"}));

  ASSERT_EQ(getResult().size(), 3);
  DeclEntry D1 = getResult()[0];
  DeclEntry D2 = getResult()[1];
  DeclEntry D3 = getResult()[2];

  EXPECT_TRUE(D1.IsDefinition);
  EXPECT_TRUE(D2.IsDefinition);
  EXPECT_TRUE(D3.IsDefinition);
  EXPECT_EQ(D1.ImplHash, D3.ImplHash);
  EXPECT_NE(D1.ImplHash, D2.ImplHash);
}


StringRef AnonymousStructS = R"c(
struct {
  int a;
  int b;
};
)c";


TEST_F(MatcherTest, AnonymousStruct) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(AnonymousStructS));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);
  EXPECT_EQ(D.Storage, StorageClass::SC_None);

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_TRUE(verifyRangeMatched(AnonymousStructS, D.NameRange, ""));
  EXPECT_EQ(D.InfRange, Range(0, 0));
  EXPECT_TRUE(verifyRangeMatched(AnonymousStructS, D.FullRange,
                                 AnonymousStructS.ltrim()));

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "{", "int", "a", ";", "int", "b", ";", "}", ";"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_TRUE(D.isAnonymous);
}


StringRef NestedStruct = R"c(
struct S1 {
    struct S2 {
        int x;
    } a;
};
)c";


TEST_F(MatcherTest, NestedStruct) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(NestedStruct));
  ASSERT_EQ(getResult().size(), 2);

  const auto &D1 = getResult()[0];
  const auto &D2 = getResult()[1];
};


StringRef StructInFunction = R"c(
void func() {
  struct S {
    int a;
    int b;
    struct S2 {
      int x;
    } c;
  };
}
)c";


TEST_F(MatcherTest, StructInFunction) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(StructInFunction));
  // local struct should be ignored
  ASSERT_EQ(getResult().size(), 0);
}


StringRef StructExpandFromMacro = R"c(
#define MACRO1 int x; struct S { int a; int b; }
MACRO1;
)c";


TEST_F(MatcherTest, StructExpandFromMacro) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(StructExpandFromMacro));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);
  EXPECT_EQ(D.Storage, StorageClass::SC_None);

  // EXPECT_EQ(D.Expansion, "struct S { int a; int b; };\n");
  // EXPECT_TRUE(verifyRangeMatched(StructExpandFromMacro,
  //   D.ExpansionReplaced, "MACRO1;\n"));
  //
  // EXPECT_TRUE(verifyRangeMatched(D.Expansion, D.NameRange, "S"));
  // EXPECT_TRUE(verifyRangeMatched(D.Expansion, D.FullRange, D.Expansion));
  //
  // EXPECT_EQ(D.ImplHash, getHashForStringList(
  //             {"struct", "S", "{", "int", "a", ";", "int", "b", ";", "}", ";"}
  //           ));
  //
  // EXPECT_TRUE(D.IsDefinition);
  // EXPECT_FALSE(D.IsInline);
  // EXPECT_FALSE(D.isAnonymous);
}







StringRef StructSWithSelfPtr = R"c(
struct S{
  int a;
  struct S * ps;
};
)c";



} // namespace clang::class_wrapper
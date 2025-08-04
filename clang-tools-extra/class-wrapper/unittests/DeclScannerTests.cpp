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
#include <source_location>

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
  MatcherCallback<TypedefDecl, DeclScanner::TypedefDeclID> TypedefDeclHandler;
  const CompilerInstance * CI = nullptr;
  std::unique_ptr<MacroExpansionRecorder> MacroRecorder;

  bool handleBeginSource(CompilerInstance &CI) override {
    this->CI = &CI;
    // MacroRecorder = std::make_unique<MacroExpansionRecorder>(CI.getLangOpts());
    // MacroRecorder->registerForPreprocessor(CI.getPreprocessor());
    return true;
  }

protected:
  MatcherTest() : RecordDeclHandler(*this), TypedefDeclHandler(*this) {
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

template <>
void MatcherTest::EnableMatcher<TypedefDecl>() {
  Finder.addMatcher(DeclScanner::TypedefDeclMatcher, &TypedefDeclHandler);
}



static StringRef SimpleStruct = R"c(
struct S {
  int a;
  int b;
};
)c";

TEST_F(MatcherTest, SimpleStruct) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(SimpleStruct, "a.c"));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.FilePath, "a.c");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);
  EXPECT_FALSE(D.IsStatic);

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_EQ(D.InfRange, Range(0, 0));
  EXPECT_TRUE(verifyRangeMatched(SimpleStruct, D.FullRange,
                                 SimpleStruct.ltrim()));

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "S", "{", "int", "a", ";", "int", "b", ";", "}"}
            ));

  EXPECT_FALSE(D.NeedExpansion);
  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsAnonymous);
  EXPECT_FALSE(D.IsUnion);
}


static StringRef SimpleUnion = R"c(
union U {
  unsigned a;
  short b;
};
)c";


TEST_F(MatcherTest, SimpleUnion) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(SimpleUnion, "a.c"));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "U");
  EXPECT_EQ(D.FilePath, "a.c");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_EQ(D.InfRange, Range(0, 0));
  EXPECT_TRUE(verifyRangeMatched(SimpleUnion, D.FullRange,
                                 SimpleUnion.ltrim()));

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"union", "U", "{", "unsigned", "a", ";", "short", "b", ";", "}"}
            ));

  EXPECT_FALSE(D.NeedExpansion);
  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsAnonymous);
  EXPECT_TRUE(D.IsUnion);
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

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_EQ(D.InfRange, Range(0, 0));
  EXPECT_TRUE(verifyRangeMatched(SimpleStructDecl, D.FullRange,
                                 SimpleStructDecl.ltrim()));

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, hash_code(0));

  EXPECT_FALSE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsAnonymous);
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

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_EQ(D.InfRange, Range(0, 0));
  EXPECT_TRUE(verifyRangeMatched(StructWithComments, D.FullRange,
                                 "// Comment 1", "// Comment3\n"));

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "S", "{", "int", "a", ";", "int", "b", ";", "}"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsAnonymous);
}


StringRef StructWithIfdef = R"c(
struct S{
int a;
#ifdef MACRO
char x;
#endif
  int b;};
)c";


TEST_F(MatcherTest, StructWithIfdef) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(StructWithIfdef));
  ASSERT_TRUE(scanOnCode(StructWithIfdef, "b.c", {"-DMACRO"}));
  ASSERT_TRUE(scanOnCode(StructWithIfdef, "c.c", {"-DMACRO2"}));

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

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_EQ(D.InfRange, Range(0, 0));
  EXPECT_TRUE(verifyRangeMatched(AnonymousStructS, D.FullRange,
                                 AnonymousStructS.ltrim()));

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "{", "int", "a", ";", "int", "b", ";", "}"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_TRUE(D.IsAnonymous);
}


StringRef NestedStruct = R"c(
struct S1 {
    struct S2 {
        int x;
    } a;
  int y;
};
)c";


TEST_F(MatcherTest, NestedStruct) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(NestedStruct));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();
  EXPECT_EQ(D.Name, "S1");
  EXPECT_TRUE(verifyRangeMatched(NestedStruct, D.FullRange,
                               NestedStruct.ltrim()));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "S1", "{", "struct", "S2", "{", "int", "x", ";", "}",
              "a", ";", "int", "y", ";", "}"} ));
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


StringRef StructWithMacro = R"c(
#define MACRO1 int x;
struct S {
  int a;
  MACRO1
  int b;
};
)c";


TEST_F(MatcherTest, StructWithMacro) {
  EnableMatcher<RecordDecl>();
  ASSERT_TRUE(scanOnCode(StructWithMacro));

  ASSERT_EQ(getResult().size(), 1);
  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_TRUE(verifyRangeMatched(StructWithMacro, D.FullRange,
                                 "struct S {", "};\n"));

  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "S", "{", "int", "a", ";", "int", "x", ";", "int", "b", ";", "}"}
            ));

  EXPECT_FALSE(D.NeedExpansion);
  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsAnonymous);
}


StringRef StructFromMacro = R"c(
#define MACRO1(x, y) struct S {int x, y;}
MACRO1(a, b);
)c";


TEST_F(MatcherTest, StructFromMacro) {
  EnableMatcher<RecordDecl>();
  ASSERT_TRUE(scanOnCode(StructFromMacro));

  ASSERT_EQ(getResult().size(), 1);
  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsAnonymous);

  EXPECT_TRUE(D.NeedExpansion);
  EXPECT_TRUE(verifyRangeMatched(StructFromMacro, D.ExpansionReplaced,
                                 "MACRO1(a, b);\n"));
}


StringRef StructPartialFromMacro = R"c(
#define MACRO1(x) int a; struct x
MACRO1(S) {
  int b;
  int c;
};

int y;
)c";


TEST_F(MatcherTest, StructPartialFromMacro) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(StructPartialFromMacro));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  EXPECT_TRUE(D.Expansion.empty());

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsAnonymous);

  EXPECT_TRUE(D.NeedExpansion);
  EXPECT_TRUE(verifyRangeMatched(StructPartialFromMacro, D.ExpansionReplaced,
                                 "MACRO1(S) {\n", "};\n"));
}


StringRef StructInMacro = R"c(
#define MACRO1 int x;
#define MACRO2(a, b) struct S { int a; int b; };
#define MACRO3(a) MACRO1 MACRO2(a, bb)
MACRO3(aaa);
)c";


TEST_F(MatcherTest, StructInMacro) {
  EnableMatcher<RecordDecl>();

  ASSERT_TRUE(scanOnCode(StructInMacro));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  EXPECT_TRUE(D.NeedExpansion);
  EXPECT_TRUE(verifyRangeMatched(StructInMacro, D.ExpansionReplaced,
                                 "MACRO3(aaa);\n"));


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
  // EXPECT_FALSE(D.IsAnonymous);
}


static void CheckTypeRef(
    const DeclEntry::MapType Map, ArrayRef<StringRef> Expected,
    const std::source_location &location = std::source_location::current()) {
  std::string locationStr = std::format(
      " at {}:{}", location.file_name(), location.line());
  ASSERT_EQ(Map.size(), Expected.size()) << locationStr;
  for (StringRef Type: Expected) {
    auto It = Map.find_as(Type);
    ASSERT_TRUE(It != Map.end()) << std::format(
        "'{}' not found{}", Type.str(), locationStr);
    EXPECT_EQ(It->second.Kind, Decl::Kind::Typedef) << locationStr;
  }
}


StringRef SimpleTypedef = R"c(
typedef int int_t;
typedef short * short_pt;
typedef char ** char_p2t;
typedef volatile unsigned long long * const ull_pt;
)c";


TEST_F(MatcherTest, SimpleTypedef) {
  EnableMatcher<TypedefDecl>();

  ASSERT_TRUE(scanOnCode(SimpleTypedef));
  ASSERT_EQ(getResult().size(), 4);

  const auto &D1 = getResult()[0];

  EXPECT_EQ(D1.Name, "int_t");
  EXPECT_EQ(D1.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D1.Expansion, "int");
  EXPECT_TRUE(verifyRangeMatched(SimpleTypedef, D1.FullRange,
                                 "typedef int int_t;\n"));
  CheckTypeRef(D1.ImplRefs, {});

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "short_pt");
  EXPECT_EQ(D2.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D2.Expansion, "short *");
  EXPECT_TRUE(verifyRangeMatched(SimpleTypedef, D2.FullRange,
                                 "typedef short * short_pt;\n"));
  CheckTypeRef(D2.ImplRefs, {});

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "char_p2t");
  EXPECT_EQ(D3.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D3.Expansion, "char **");
  EXPECT_TRUE(verifyRangeMatched(SimpleTypedef, D3.FullRange,
                                 "typedef char ** char_p2t;\n"));
  CheckTypeRef(D3.ImplRefs, {});

  const auto &D4 = getResult()[3];
  EXPECT_EQ(D4.Name, "ull_pt");
  EXPECT_EQ(D4.Expansion, "volatile unsigned long long *const");
  EXPECT_TRUE(verifyRangeMatched(SimpleTypedef, D4.FullRange,
    "typedef volatile unsigned long long * const ull_pt;\n"));
  CheckTypeRef(D4.ImplRefs, {});
}


StringRef TypedefOnCustomType = R"c(
typedef int foo_t;
typedef foo_t bar_t;
typedef bar_t * bar_p;
typedef bar_p foo_p;
)c";


TEST_F(MatcherTest, TypedefOnCustomType) {
  EnableMatcher<TypedefDecl>();

  ASSERT_TRUE(scanOnCode(TypedefOnCustomType));
  ASSERT_EQ(getResult().size(), 4);

  const auto &D1 = getResult()[0];
  EXPECT_EQ(D1.Name, "foo_t");
  EXPECT_EQ(D1.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D1.Expansion, "int");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnCustomType, D1.FullRange,
                                 "typedef int foo_t;\n"));
  CheckTypeRef(D1.ImplRefs, {});

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "bar_t");
  EXPECT_EQ(D2.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D2.Expansion, "foo_t");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnCustomType, D2.FullRange,
                                 "typedef foo_t bar_t;\n"));
  CheckTypeRef(D2.ImplRefs, {"foo_t"});

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "bar_p");
  EXPECT_EQ(D3.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D3.Expansion, "bar_t *");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnCustomType, D3.FullRange,
                                 "typedef bar_t * bar_p;\n"));
  CheckTypeRef(D3.ImplRefs, {"bar_t"});

  const auto &D4 = getResult()[3];
  EXPECT_EQ(D4.Name, "foo_p");
  EXPECT_EQ(D4.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D4.Expansion, "bar_p");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnCustomType, D4.FullRange,
                                 "typedef bar_p foo_p;\n"));
  CheckTypeRef(D4.ImplRefs, {"bar_p"});
}


StringRef TypedefOnVoidPtr = R"c(
typedef void void_t;
typedef void * void_p;
typedef void_t void_t2;
typedef void_t2 * void_p2;
typedef void_p2 void_p3;
)c";


TEST_F(MatcherTest, TypedefOnVoidPtr) {
  EnableMatcher<TypedefDecl>();

  ASSERT_TRUE(scanOnCode(TypedefOnVoidPtr));
  ASSERT_EQ(getResult().size(), 5);

  const auto &D1 = getResult()[0];
  EXPECT_EQ(D1.Name, "void_t");
  EXPECT_EQ(D1.Expansion, "void");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnVoidPtr, D1.FullRange,
    "typedef void void_t;\n"));
  CheckTypeRef(D1.ImplRefs, {});

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "void_p");
  EXPECT_EQ(D2.Expansion, "void *");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnVoidPtr, D2.FullRange,
    "typedef void * void_p;\n"));
  CheckTypeRef(D2.ImplRefs, {});

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "void_t2");
  EXPECT_EQ(D3.Expansion, "void_t");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnVoidPtr, D3.FullRange,
    "typedef void_t void_t2;\n"));
  CheckTypeRef(D3.ImplRefs, {"void_t"});

  const auto &D4 = getResult()[3];
  EXPECT_EQ(D4.Name, "void_p2");
  EXPECT_EQ(D4.Expansion, "void_t2 *");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnVoidPtr, D4.FullRange,
    "typedef void_t2 * void_p2;\n"));
  CheckTypeRef(D4.ImplRefs, {"void_t2"});

  const auto &D5 = getResult()[4];
  EXPECT_EQ(D5.Name, "void_p3");
  EXPECT_EQ(D5.Expansion, "void_p2");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnVoidPtr, D5.FullRange,
    "typedef void_p2 void_p3;\n"));
  CheckTypeRef(D5.ImplRefs, {"void_p2"});
}


StringRef TypedefOnArray = R"c(
typedef int foo_t;
typedef foo_t a_t[];
typedef foo_t a1_t[10];
typedef foo_t a2_t[][20];
typedef foo_t a3_t[10][20];
typedef foo_t * a1_p_t[][10];
typedef a_t bar_t;
)c";


TEST_F(MatcherTest, TypedefOnArray) {
  EnableMatcher<TypedefDecl>();

  ASSERT_TRUE(scanOnCode(TypedefOnArray));
  ASSERT_EQ(getResult().size(), 7);

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "a_t");
  EXPECT_EQ(D2.Expansion, "foo_t[]");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnArray, D2.FullRange,
    "typedef foo_t a_t[];\n"));
  CheckTypeRef(D2.ImplRefs, {"foo_t"});

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "a1_t");
  EXPECT_EQ(D3.Expansion, "foo_t[10]");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnArray, D3.FullRange,
    "typedef foo_t a1_t[10];\n"));
  CheckTypeRef(D3.ImplRefs, {"foo_t"});

  const auto &D4 = getResult()[3];
  EXPECT_EQ(D4.Name, "a2_t");
  EXPECT_EQ(D4.Expansion, "foo_t[][20]");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnArray, D4.FullRange,
    "typedef foo_t a2_t[][20];\n"));
  CheckTypeRef(D4.ImplRefs, {"foo_t"});

  const auto &D5 = getResult()[4];
  EXPECT_EQ(D5.Name, "a3_t");
  EXPECT_EQ(D5.Expansion, "foo_t[10][20]");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnArray, D5.FullRange,
    "typedef foo_t a3_t[10][20];\n"));
  CheckTypeRef(D5.ImplRefs, {"foo_t"});

  const auto &D6 = getResult()[5];
  EXPECT_EQ(D6.Name, "a1_p_t");
  EXPECT_EQ(D6.Expansion, "foo_t *[][10]");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnArray, D6.FullRange,
    "typedef foo_t * a1_p_t[][10];\n"));
  CheckTypeRef(D6.ImplRefs, {"foo_t"});

  const auto &D7 = getResult()[6];
  EXPECT_EQ(D7.Name, "bar_t");
  EXPECT_EQ(D7.Expansion, "a_t");
  EXPECT_TRUE(verifyRangeMatched(TypedefOnArray, D7.FullRange,
    "typedef a_t bar_t;\n"));
  CheckTypeRef(D7.ImplRefs, {"a_t"});
}


StringRef TypedefToStruct = R"c(
struct S {
  int a;
};
typedef struct S S_t;
typedef struct S * S_p;
)c";


TEST_F(MatcherTest, TypedefToStruct) {
  EnableMatcher<TypedefDecl>();

  ASSERT_TRUE(scanOnCode(TypedefToStruct));
  ASSERT_EQ(getResult().size(), 2);

  const auto &D1 = getResult()[0];
  EXPECT_EQ(D1.Name, "S_t");
  EXPECT_EQ(D1.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D1.Expansion, "struct S");
  EXPECT_TRUE(verifyRangeMatched(TypedefToStruct, D1.FullRange,
                                 "typedef struct S S_t;\n"));
  ASSERT_EQ(D1.ImplRefs.size(), 1);
  CheckTypeRef(D1.ImplRefs, {"S"});

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "S_p");
  EXPECT_EQ(D2.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D2.Expansion, "struct S *");
  EXPECT_TRUE(verifyRangeMatched(TypedefToStruct, D2.FullRange,
                                 "typedef struct S * S_p;\n"));
  // Note: struct S in not necessary for S_p, since it is used as a pointer
  CheckTypeRef(D2.ImplRefs, {});
}


StringRef StructSWithSelfPtr = R"c(
struct S{
  int a;
  struct S * ps;
};
)c";



} // namespace clang::class_wrapper
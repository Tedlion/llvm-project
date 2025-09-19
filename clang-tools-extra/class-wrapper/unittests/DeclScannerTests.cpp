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
#include "gtest/gtest.h"

#include "DeclScanner.h"

#include "../Support.h"

#include <ranges>
#include <source_location>

using namespace clang;
using namespace clang::ast_matchers;

namespace clang::class_wrapper {


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


class MatcherTest : public ::testing::Test {
  DeclScanner Scanner;
  std::string ErrorMessage;
  StringRef SourceCode;
  std::string PreProcessed;

protected:
  template <typename T>
  void enableMatcher() {
    Scanner.enableMatcher<T>();
  }


  bool scanOnCode(StringRef Code, StringRef Filename = "input.c",
                  std::vector<std::string> CompileArgs = {}) {
    SourceCode = Code;
    raw_string_ostream OS(PreProcessed);
    auto PDAction = PrintPreprocessedAndDeps(OS);
    if (!runToolOnCodeWithArgs(PDAction.newFactory()->create(),
                               Code, CompileArgs, Filename)) {
      ErrorMessage = "PDAction failed\n";
      return false;
    }

    if (llvm::find(CompileArgs, "-target") == CompileArgs.end()) {
      CompileArgs.push_back("-target");
      CompileArgs.push_back("i386-unknown-unknown");
    }
    CompileArgs.push_back("-fparse-all-comments");
    CompileArgs.push_back("-w");

    std::unique_ptr<FrontendActionFactory> SourceFactory(
        newFrontendActionFactory(&Scanner.getSourceFinder(), &Scanner));

    if (!runToolOnCodeWithArgs(SourceFactory->create(),
                               Code, CompileArgs, Filename)) {
      ErrorMessage = std::format("Parsing error in \"{}\"", Code);
      return false;
    }

    CompileArgs.push_back("-x");
    CompileArgs.push_back("c");

    std::unique_ptr<FrontendActionFactory> PPFactory(
        newFrontendActionFactory(&Scanner.getPreprocessedFinder(), &Scanner));

    if (!runToolOnCodeWithArgs(PPFactory->create(),
                               PreProcessed, CompileArgs, Filename + ".i")) {
      ErrorMessage = std::format("Parsing error in \"{}\"", PreProcessed);
      return false;
    }

    return true;
  }


  [[nodiscard]] const std::vector<DeclEntry> &getResult() const {
    return Scanner.getDeclEntries();
  }


  [[nodiscard]] const std::string &getError() const {
    return ErrorMessage;
  }


  void checkToRemove(const DeclEntry &DE, StringRef Expected) const {
    EXPECT_TRUE(verifyRangeMatched(SourceCode, DE.ToRemove, Expected));
  }


  void checkToRemove(const DeclEntry &DE, StringRef ExpectedBegin,
                     StringRef ExpectedEnd) const {
    EXPECT_TRUE(verifyRangeMatched(SourceCode, DE.ToRemove,
      ExpectedBegin, ExpectedEnd));
  }


  void checkAddToClass(const DeclEntry &DE, StringRef Expected) const {
    if (DE.AddToClassText.empty()) {
      EXPECT_TRUE(verifyRangeMatched(PreProcessed, DE.AddToClass, Expected));
    } else {
      EXPECT_EQ(DE.AddToClassText, Expected);
    }
  }


  void checkAddToClass(const DeclEntry &DE, StringRef ExpectedBegin,
                       StringRef ExpectedEnd) const {
    if (DE.AddToClassText.empty()) {
      EXPECT_TRUE(verifyRangeMatched(PreProcessed, DE.AddToClass,
        ExpectedBegin, ExpectedEnd));
    } else {
      EXPECT_TRUE(DE.AddToClassText.starts_with(ExpectedBegin));
      EXPECT_TRUE(DE.AddToClassText.ends_with(ExpectedEnd));
    }
  }
};


static hash_code getHashForStringList(ArrayRef<StringRef> Strings) {
  hash_code Hash = 0;
  for (const auto &S : Strings) {
    Hash = hash_combine(Hash, S);
  }
  return Hash;
}


using std::source_location;


static std::string getLocationStr(
    const source_location &Location = source_location::current()) {
  return std::format(" at {}:{}", Location.file_name(), Location.line());
}


static void checkRefs(
    const DeclEntry::MapType &Map, ArrayRef<StringRef> Expected,
    const source_location &Location = source_location::current()) {
  std::string LocationStr = getLocationStr(Location);
  ASSERT_EQ(Map.size(), Expected.size()) << LocationStr;
  for (StringRef Type : Expected) {
    auto It = Map.find_as(Type);
    ASSERT_TRUE(It != Map.end()) << std::format(
                                    "'{}' not found{}", Type.str(),
                                    LocationStr);
    // EXPECT_EQ(It->second.Kind, Decl::Kind::Typedef) << LocationStr;
  }
}


static void checkEditLocations(
    const DeclEntry &DE, ArrayRef<EditLocation> Expected,
    const source_location &Location = source_location::current()) {
  std::string LocationStr = getLocationStr(Location);
  EXPECT_EQ(DE.EditLocations.size(), Expected.size())
      << "Edit locations size mismatch" << LocationStr;
  for (auto [Actual, Expect] : std::views::zip(DE.EditLocations, Expected)) {
    EXPECT_EQ(Actual, Expect) << "Edit location mismatch" << LocationStr;
  }
}


class TypedefDeclTest : public MatcherTest {
protected:
  void SetUp() override {
    MatcherTest::SetUp();
    enableMatcher<TypedefDecl>();
  }
};


static StringRef SimpleTypedef = R"c(
typedef int int_t;
typedef short * short_pt;
typedef char ** char_p2t;
typedef volatile unsigned long long * const ull_pt;
)c";


TEST_F(TypedefDeclTest, Simple) {
  ASSERT_TRUE(scanOnCode(SimpleTypedef));
  ASSERT_EQ(getResult().size(), 4);

  const auto &D1 = getResult()[0];

  EXPECT_EQ(D1.Name, "int_t");
  EXPECT_EQ(D1.Kind, Decl::Kind::Typedef);
  checkToRemove(D1, "typedef int int_t;\n");
  checkAddToClass(D1, "typedef int int_t;\n");
  checkRefs(D1.ImplRefs, {});
  EXPECT_EQ(D1.ImplHash, getHashForStringList({"typedef", "int", "int_t"}));

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "short_pt");
  EXPECT_EQ(D2.Kind, Decl::Kind::Typedef);
  checkToRemove(D2, "typedef short * short_pt;\n");
  checkAddToClass(D2, "typedef short * short_pt;\n");
  checkRefs(D2.ImplRefs, {});
  EXPECT_EQ(D2.ImplHash,
            getHashForStringList( {"typedef", "short", "*", "short_pt"}));

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "char_p2t");
  EXPECT_EQ(D3.Kind, Decl::Kind::Typedef);
  checkToRemove(D3, "typedef char ** char_p2t;\n");
  checkAddToClass(D3, "typedef char ** char_p2t;\n");
  checkRefs(D3.ImplRefs, {});
  EXPECT_EQ(D3.ImplHash, getHashForStringList(
              {"typedef", "char", "*", "*", "char_p2t"}));

  const auto &D4 = getResult()[3];
  EXPECT_EQ(D4.Name, "ull_pt");
  checkToRemove(D4, "typedef volatile unsigned long long * const ull_pt;\n");
  checkAddToClass(D4, "typedef volatile unsigned long long * const ull_pt;\n");
  checkRefs(D4.ImplRefs, {});
  EXPECT_EQ(D4.ImplHash, getHashForStringList( {"typedef", "volatile",
              "unsigned", "long", "long", "*", "const", "ull_pt"}));
}


static StringRef TypedefOnCustomType = R"c(
typedef int foo_t;
typedef foo_t bar_t;
typedef bar_t * bar_p;
typedef bar_p foo_p;
)c";


TEST_F(TypedefDeclTest, OnCustomType) {
  ASSERT_TRUE(scanOnCode(TypedefOnCustomType));
  ASSERT_EQ(getResult().size(), 4);

  const auto &D1 = getResult()[0];
  EXPECT_EQ(D1.Name, "foo_t");
  EXPECT_EQ(D1.Kind, Decl::Kind::Typedef);
  checkToRemove(D1, "typedef int foo_t;\n");
  checkAddToClass(D1, "typedef int foo_t;\n");
  checkRefs(D1.ImplRefs, {});

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "bar_t");
  EXPECT_EQ(D2.Kind, Decl::Kind::Typedef);
  checkToRemove(D2, "typedef foo_t bar_t;\n");
  checkAddToClass(D2, "typedef foo_t bar_t;\n");
  checkRefs(D2.ImplRefs, {"foo_t"});

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "bar_p");
  EXPECT_EQ(D3.Kind, Decl::Kind::Typedef);
  checkToRemove(D3, "typedef bar_t * bar_p;\n");
  checkAddToClass(D3, "typedef bar_t * bar_p;\n");
  checkRefs(D3.ImplRefs, {"bar_t"});

  const auto &D4 = getResult()[3];
  EXPECT_EQ(D4.Name, "foo_p");
  EXPECT_EQ(D4.Kind, Decl::Kind::Typedef);
  checkToRemove(D4, "typedef bar_p foo_p;\n");
  checkAddToClass(D4, "typedef bar_p foo_p;\n");
  checkRefs(D4.ImplRefs, {"bar_p"});
}


static StringRef TypedefOnVoidPtr = R"c(
typedef void void_t;
typedef void * void_p;
typedef void_t void_t2;
typedef void_t2 * void_p2;
typedef void_p2 void_p3;
)c";


TEST_F(TypedefDeclTest, OnVoidPtr) {
  ASSERT_TRUE(scanOnCode(TypedefOnVoidPtr));
  ASSERT_EQ(getResult().size(), 5);

  const auto &D1 = getResult()[0];
  EXPECT_EQ(D1.Name, "void_t");
  checkToRemove(D1, "typedef void void_t;\n");
  checkAddToClass(D1, "typedef void void_t;\n");
  checkRefs(D1.ImplRefs, {});

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "void_p");
  checkToRemove(D2, "typedef void * void_p;\n");
  checkAddToClass(D2, "typedef void * void_p;\n");
  checkRefs(D2.ImplRefs, {});

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "void_t2");
  checkToRemove(D3, "typedef void_t void_t2;\n");
  checkAddToClass(D3, "typedef void_t void_t2;\n");
  checkRefs(D3.ImplRefs, {"void_t"});

  const auto &D4 = getResult()[3];
  EXPECT_EQ(D4.Name, "void_p2");
  checkToRemove(D4, "typedef void_t2 * void_p2;\n");
  checkAddToClass(D4, "typedef void_t2 * void_p2;\n");
  checkRefs(D4.ImplRefs, {"void_t2"});

  const auto &D5 = getResult()[4];
  EXPECT_EQ(D5.Name, "void_p3");
  checkToRemove(D5, "typedef void_p2 void_p3;\n");
  checkAddToClass(D5, "typedef void_p2 void_p3;\n");
  checkRefs(D5.ImplRefs, {"void_p2"});
}


static StringRef TypedefOnArray = R"c(
typedef int foo_t;
typedef foo_t a_t[];
typedef foo_t a1_t[10];
typedef foo_t a2_t[][20];
typedef foo_t a3_t[10][20];
typedef foo_t * a1_p_t[][10];
typedef a_t bar_t;
)c";


TEST_F(TypedefDeclTest, OnArray) {
  ASSERT_TRUE(scanOnCode(TypedefOnArray));
  ASSERT_EQ(getResult().size(), 7);

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "a_t");
  checkToRemove(D2, "typedef foo_t a_t[];\n");
  checkAddToClass(D2, "typedef foo_t a_t[];\n");
  checkRefs(D2.ImplRefs, {"foo_t"});
  checkEditLocations(D2, {});

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "a1_t");
  checkToRemove(D3, "typedef foo_t a1_t[10];\n");
  checkAddToClass(D3, "typedef foo_t a1_t[10];\n");
  checkRefs(D3.ImplRefs, {"foo_t"});
  checkEditLocations(D3, {});

  const auto &D4 = getResult()[3];
  EXPECT_EQ(D4.Name, "a2_t");
  checkToRemove(D4, "typedef foo_t a2_t[][20];\n");
  checkAddToClass(D4, "typedef foo_t a2_t[][20];\n");
  checkRefs(D4.ImplRefs, {"foo_t"});
  checkEditLocations(D4, {});

  const auto &D5 = getResult()[4];
  EXPECT_EQ(D5.Name, "a3_t");
  checkToRemove(D5, "typedef foo_t a3_t[10][20];\n");
  checkAddToClass(D5, "typedef foo_t a3_t[10][20];\n");
  checkRefs(D5.ImplRefs, {"foo_t"});
  checkEditLocations(D5, {});

  const auto &D6 = getResult()[5];
  EXPECT_EQ(D6.Name, "a1_p_t");
  checkToRemove(D6, "typedef foo_t * a1_p_t[][10];\n");
  checkAddToClass(D6, "typedef foo_t * a1_p_t[][10];\n");
  checkRefs(D6.ImplRefs, {"foo_t"});
  checkEditLocations(D6, {});

  const auto &D7 = getResult()[6];
  EXPECT_EQ(D7.Name, "bar_t");
  checkToRemove(D7, "typedef a_t bar_t;\n");
  checkAddToClass(D7, "typedef a_t bar_t;\n");
  checkRefs(D7.ImplRefs, {"a_t"});
  checkEditLocations(D7, {});
}


static StringRef TypedefOnStruct = R"c(
struct S {
  int a;
};
typedef struct S S_t;
typedef struct S * S_p;
)c";


TEST_F(TypedefDeclTest, OnStruct) {
  ASSERT_TRUE(scanOnCode(TypedefOnStruct));
  ASSERT_EQ(getResult().size(), 2);

  const auto &D1 = getResult()[0];
  EXPECT_EQ(D1.Name, "S_t");
  EXPECT_EQ(D1.Kind, Decl::Kind::Typedef);
  checkToRemove(D1, "typedef struct S S_t;\n");
  checkAddToClass(D1, "typedef struct S S_t;\n");
  checkRefs(D1.ImplRefs, {"S"});

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "S_p");
  EXPECT_EQ(D2.Kind, Decl::Kind::Typedef);
  checkToRemove(D2, "typedef struct S * S_p;\n");
  checkAddToClass(D2, "typedef struct S * S_p;\n");
  // Note: struct S in not necessary for S_p, since it is used as a pointer
  checkRefs(D2.ImplRefs, {});
}


static StringRef TypedefOnFunctionPtr = R"c(
typedef int foo_t;
typedef int (*(*fptr)(int))(int[5]);
typedef int (*(*fptr2)(int))(foo_t (*[10])(int));
typedef int (*(*fptr3[10])(int))(foo_t (*[10])(int));
typedef int (*(*(*fptr4[10])(int))(foo_t (*[10])(int)))();
typedef int (*(*fptr5[10])(int))(fptr2[8]);
typedef int (*(*(*fptr6[10])(int))(foo_t (*[10])(int)))(struct S);
typedef struct S (*(*(*fptr7[10])(int))(foo_t (*[10])(int)))();
)c";


TEST_F(TypedefDeclTest, OnFunctionPtr) {
  constexpr auto InsertClass = EditKind::InsertClassName;

  ASSERT_TRUE(scanOnCode(TypedefOnFunctionPtr));
  ASSERT_EQ(getResult().size(), 8);

  const auto &D1 = getResult()[1];
  EXPECT_EQ(D1.Name, "fptr");
  EXPECT_EQ(D1.Kind, Decl::Kind::Typedef);
  checkToRemove(D1, "typedef int (*(*fptr)(int))(int[5]);\n");
  checkAddToClass(D1, "typedef int (*(*fptr)(int))(int[5]);\n");
  checkRefs(D1.ImplRefs, {});
  checkEditLocations(D1, {EditLocation(InsertClass, 13),
                          EditLocation(InsertClass, 15)});

  const auto &D2 = getResult()[2];
  EXPECT_EQ(D2.Name, "fptr2");
  EXPECT_EQ(D2.Kind, Decl::Kind::Typedef);
  checkToRemove(D2, "typedef int (*(*fptr2)(int))(foo_t (*[10])(int));\n");
  checkAddToClass(D2, "typedef int (*(*fptr2)(int))(foo_t (*[10])(int));\n");
  checkRefs(D2.ImplRefs, {"foo_t"});
  checkEditLocations(D2, {EditLocation(InsertClass, 13),
                          EditLocation(InsertClass, 15),
                          EditLocation(InsertClass, 36)});

  const auto &D3 = getResult()[3];
  EXPECT_EQ(D3.Name, "fptr3");
  EXPECT_EQ(D3.Kind, Decl::Kind::Typedef);
  checkToRemove(D3, "typedef int (*(*fptr3[10])(int))(foo_t (*[10])(int));\n");
  checkAddToClass(
      D3, "typedef int (*(*fptr3[10])(int))(foo_t (*[10])(int));\n");
  checkRefs(D3.ImplRefs, {"foo_t"});
  checkEditLocations(D3, {EditLocation(InsertClass, 13),
                          EditLocation(InsertClass, 15),
                          EditLocation(InsertClass, 40)});

  const auto &D4 = getResult()[4];
  EXPECT_EQ(D4.Name, "fptr4");
  EXPECT_EQ(D4.Kind, Decl::Kind::Typedef);
  checkToRemove(
      D4, "typedef int (*(*(*fptr4[10])(int))(foo_t (*[10])(int)))();\n");
  checkAddToClass(
      D4, "typedef int (*(*(*fptr4[10])(int))(foo_t (*[10])(int)))();\n");
  checkRefs(D4.ImplRefs, {"foo_t"});
  checkEditLocations(D4, {EditLocation(InsertClass, 13),
                          EditLocation(InsertClass, 15),
                          EditLocation(InsertClass, 17),
                          EditLocation(InsertClass, 42),
                     });

  const auto &D5 = getResult()[5];
  EXPECT_EQ(D5.Name, "fptr5");
  EXPECT_EQ(D5.Kind, Decl::Kind::Typedef);
  checkToRemove(D5, "typedef int (*(*fptr5[10])(int))(fptr2[8]);\n");
  checkAddToClass(D5, "typedef int (*(*fptr5[10])(int))(fptr2[8]);\n");
  checkRefs(D5.ImplRefs, {"fptr2"});
  checkEditLocations(D5, {EditLocation(InsertClass, 13),
                          EditLocation(InsertClass, 15)});

  const auto &D6 = getResult()[6];
  EXPECT_EQ(D6.Name, "fptr6");
  EXPECT_EQ(D6.Kind, Decl::Kind::Typedef);
  checkToRemove(
      D6,
      "typedef int (*(*(*fptr6[10])(int))(foo_t (*[10])(int)))(struct S);\n");
  checkAddToClass(
      D6,
      "typedef int (*(*(*fptr6[10])(int))(foo_t (*[10])(int)))(struct S);\n");
  checkRefs(D6.ImplRefs, {"foo_t", "S"});
  checkEditLocations(D6, {EditLocation(InsertClass, 13),
                          EditLocation(InsertClass, 15),
                          EditLocation(InsertClass, 17),
                          EditLocation(InsertClass, 42)});

  const auto &D7 = getResult()[7];
  EXPECT_EQ(D7.Name, "fptr7");
  EXPECT_EQ(D7.Kind, Decl::Kind::Typedef);
  checkToRemove(
      D7, "typedef struct S (*(*(*fptr7[10])(int))(foo_t (*[10])(int)))();\n");
  checkAddToClass(
      D7, "typedef struct S (*(*(*fptr7[10])(int))(foo_t (*[10])(int)))();\n");
  checkRefs(D7.ImplRefs, {"foo_t", "S"});
  checkEditLocations(D7, {EditLocation(InsertClass, 18),
                          EditLocation(InsertClass, 20),
                          EditLocation(InsertClass, 22),
                          EditLocation(InsertClass, 47)});
}


static StringRef TypedefInLocalScope = R"c(
void func() {
  typedef int local_t;
}
)c";


TEST_F(TypedefDeclTest, InLocalScope) {
  ASSERT_TRUE(scanOnCode(TypedefInLocalScope));
  ASSERT_EQ(getResult().size(), 0);
}


static StringRef TypedefFromMacro = R"c(
#define DEFINE_TYPEDEF(name, type, var) typedef type name;\
name var;
DEFINE_TYPEDEF(my_int_t, int, v)
)c";


TEST_F(TypedefDeclTest, FromMacro) {
  ASSERT_TRUE(scanOnCode(TypedefFromMacro));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "my_int_t");
  EXPECT_EQ(D.Kind, Decl::Kind::Typedef);
  checkToRemove(D, "DEFINE_TYPEDEF(my_int_t, int, v)\n");
  checkAddToClass(D, "typedef int my_int_t;");
  checkRefs(D.ImplRefs, {});
  EXPECT_EQ(D.ImplHash, getHashForStringList({"typedef", "int", "my_int_t"}));
}


class RecordDeclTest : public MatcherTest {
protected:
  void SetUp() override {
    MatcherTest::SetUp();
    enableMatcher<RecordDecl>();
  }
};


static StringRef SimpleStruct = R"c(
struct S {
  int a;
  int b;
};
)c";

TEST_F(RecordDeclTest, SimpleStruct) {
  ASSERT_TRUE(scanOnCode(SimpleStruct, "a.c"));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.SourcePath, "a.c");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);
  EXPECT_FALSE(D.IsStatic);
  checkToRemove(D, SimpleStruct.ltrim());
  checkAddToClass(D, SimpleStruct.ltrim());

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "S", "{", "int", "a", ";", "int", "b", ";", "}"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsUnnamed);
  EXPECT_FALSE(D.IsUnion);
}


static StringRef SimpleUnion = R"c(
union U {
  unsigned a;
  short b;
};
)c";


TEST_F(RecordDeclTest, SimpleUnion) {
  ASSERT_TRUE(scanOnCode(SimpleUnion, "a.c"));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "U");
  EXPECT_EQ(D.SourcePath, "a.c");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);
  checkToRemove(D, SimpleUnion.ltrim());
  checkAddToClass(D, SimpleUnion.ltrim());

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"union", "U", "{", "unsigned", "a", ";", "short", "b", ";", "}"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsUnnamed);
  EXPECT_TRUE(D.IsUnion);
}


static StringRef SimpleStructDecl = R"c(
struct S;
)c";


TEST_F(RecordDeclTest, SimpleStructDecl) {
  ASSERT_TRUE(scanOnCode(SimpleStructDecl, "a.c"));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.SourcePath, "a.c");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  checkToRemove(D, SimpleStructDecl.ltrim());
  checkAddToClass(D, "");

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, hash_code(0));

  EXPECT_FALSE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsUnnamed);
}


static StringRef StructWithComments = R"c(
// Comment 1
struct S {
  int a; // Comment 2
  int b;
}; // Comment3
)c";


TEST_F(RecordDeclTest, StructWithComments) {
  ASSERT_TRUE(scanOnCode(StructWithComments));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  checkToRemove(D, "// Comment 1", "// Comment3\n");
  checkAddToClass(D, "// Comment 1", "// Comment3\n");

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "S", "{", "int", "a", ";", "int", "b", ";", "}"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsUnnamed);
}


static StringRef StructWithIfdef = R"c(
struct S{
int a;
#ifdef MACRO
char x;
#endif
  int b;};
)c";


TEST_F(RecordDeclTest, StructWithIfdef1) {
  ASSERT_TRUE(scanOnCode(StructWithIfdef));

  ASSERT_EQ(getResult().size(), 1);
  DeclEntry D = getResult().front();

  EXPECT_EQ(D.ImplHash, getHashForStringList({"struct", "S", "{",
              "int", "a", ";", "int", "b", ";", "}"}));
}


TEST_F(RecordDeclTest, StructWithIfdef2) {
  ASSERT_TRUE(scanOnCode(StructWithIfdef, "b.c", {"-DMACRO"}));

  ASSERT_EQ(getResult().size(), 1);
  DeclEntry D = getResult().front();

  EXPECT_EQ(D.ImplHash, getHashForStringList({"struct", "S", "{",
              "int", "a", ";", "char", "x", ";", "int", "b", ";", "}"}));
}


TEST_F(RecordDeclTest, StructWithIfdef3) {
  ASSERT_TRUE(scanOnCode(StructWithIfdef, "c.c", {"-DMACRO2"}));

  ASSERT_EQ(getResult().size(), 1);
  DeclEntry D = getResult().front();

  EXPECT_EQ(D.ImplHash, getHashForStringList({"struct", "S", "{",
              "int", "a", ";", "int", "b", ";", "}"}));
}


static StringRef AnonymousStructS = R"c(
struct {
  int a;
  int b;
} s;
)c";


TEST_F(RecordDeclTest, AnonymousStruct) {
  ASSERT_TRUE(scanOnCode(AnonymousStructS));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  checkToRemove(D, AnonymousStructS.ltrim());
  checkAddToClass(D, AnonymousStructS.ltrim());

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "{", "int", "a", ";", "int", "b", ";", "}"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_TRUE(D.IsUnnamed);
}


static StringRef NestedStruct = R"c(
struct S1 {
    struct S2 {
        int x;
    } a;
  int y;
};
)c";


TEST_F(RecordDeclTest, NestedStruct) {
  ASSERT_TRUE(scanOnCode(NestedStruct));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();
  EXPECT_EQ(D.Name, "S1");
  checkToRemove(D, NestedStruct.ltrim());
  checkAddToClass(D, NestedStruct.ltrim());
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "S1", "{", "struct", "S2", "{", "int", "x", ";", "}",
              "a", ";", "int", "y", ";", "}"} ));
};


static StringRef StructInFunction = R"c(
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


TEST_F(RecordDeclTest, StructInFunction) {
  ASSERT_TRUE(scanOnCode(StructInFunction));
  // local struct should be ignored
  ASSERT_EQ(getResult().size(), 0);
}


static StringRef StructWithMacro = R"c(
#define MACRO1 int x;
struct S {
  int a;
  MACRO1
  int b;
};
)c";


TEST_F(RecordDeclTest, StructWithMacro) {
  ASSERT_TRUE(scanOnCode(StructWithMacro));

  ASSERT_EQ(getResult().size(), 1);
  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  checkToRemove(D, "struct S {\n  int a;\n  MACRO1\n  int b;\n};\n");
  checkAddToClass(D, "struct S {\n  int a;\n  int x;\n  int b;\n};\n");

  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "S", "{", "int", "a", ";", "int", "x", ";", "int", "b",
              ";", "}"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsUnnamed);
}


static StringRef StructFromMacro = R"c(
#define MACRO1(x, y) struct S {int x, y;}
MACRO1(a, b);
)c";


TEST_F(RecordDeclTest, StructFromMacro) {
  ASSERT_TRUE(scanOnCode(StructFromMacro, "dir/a.c"));

  ASSERT_EQ(getResult().size(), 1);
  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  checkToRemove(D, "MACRO1(a, b);\n");
  checkAddToClass(D, "struct S {int a, b;};\n");

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsUnnamed);
}


static StringRef StructPartialFromMacro = R"c(
#define MACRO1(x) int a; struct x
MACRO1(S) {
  int b;
  int c;
};

int y;
)c";


TEST_F(RecordDeclTest, StructPartialFromMacro) {
  ASSERT_TRUE(scanOnCode(StructPartialFromMacro));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  // The continuous newlines is preprocessed to a single newline,
  // which makes the following results a bit different.
  checkToRemove(D, "MACRO1(S) {\n", "};\n\n");
  checkAddToClass(D, "struct S {\n", "};\n");

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsUnnamed);
}


static StringRef StructInMacro = R"c(
#define MACRO1 int x;
#define MACRO2(a, b) struct S { int a; int b; };
#define MACRO3(a) MACRO1 MACRO2(a, bb)
MACRO3(aaa)
)c";


TEST_F(RecordDeclTest, StructInMacro) {
  ASSERT_TRUE(scanOnCode(StructInMacro));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  checkToRemove(D, "MACRO3(aaa)\n");
  checkAddToClass(D, "struct S { int aaa; int bb; };\n");

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsUnnamed);
}


static StringRef StructWithAttributes = R"c(
struct __attribute__((aligned(8))) S {
  char a;
  short b;
} __attribute__((packed));
)c";


TEST_F(RecordDeclTest, StructWithAttributes) {
  ASSERT_TRUE(scanOnCode(StructWithAttributes));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "S");
  EXPECT_EQ(D.Kind, Decl::Kind::Record);

  checkToRemove(D, StructWithAttributes.ltrim());
  checkAddToClass(D, StructWithAttributes.ltrim());

  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"struct", "__attribute__","(","(","aligned","(","8",")",")",")",
              "S", "{", "char", "a", ";", "short", "b", ";", "}",
              "__attribute__","(","(","packed",")",")"}
            ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsUnnamed);
}


static StringRef StructRelyOnTypes = R"c(
typedef int foo_t;
typedef short bar_t;
struct S {
  foo_t a;
  bar_t *pb;
  struct C * pc;
};
)c";


TEST_F(RecordDeclTest, StructRelyOnTypes) {
  ASSERT_TRUE(scanOnCode(StructRelyOnTypes));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();
  checkRefs(D.ImplRefs, {"foo_t", "bar_t"});
}


static StringRef NestedStructRelyOnTypes = R"c(
typedef int foo_t;
typedef short bar_t;
struct S{
  struct S * ps;
  struct S2 {
    foo_t * pf;
  } *ps2;
  struct S3 {
    bar_t b;
    struct S2 s2;
  } s3;
};
)c";


TEST_F(RecordDeclTest, NestedStructRelyOnTypes) {
  ASSERT_TRUE(scanOnCode(NestedStructRelyOnTypes));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();
  checkRefs(D.ImplRefs, {"foo_t", "bar_t"});
}


static StringRef CombinedTypedefAndStruct = R"c(
typedef struct S {
  int a;
} S_t;
typedef struct {
  short b;
} S2_t;
typedef S2_t S2_t2;
typedef struct {
  int x;
} Sa_t[][10];
)c";


TEST_F(MatcherTest, CombinedTypedefAndStruct) {
  enableMatcher<RecordDecl>();
  enableMatcher<TypedefDecl>();

  ASSERT_TRUE(scanOnCode(CombinedTypedefAndStruct));
  ASSERT_EQ(getResult().size(), 7);

  const auto &D1 = getResult()[0];
  EXPECT_EQ(D1.Name, "S");
  EXPECT_EQ(D1.Kind, Decl::Kind::Record);
  EXPECT_TRUE(D1.DeclID);
  checkToRemove(D1, "struct S {", "} S_t;\n");
  checkAddToClass(D1, "struct S {", "} S_t;\n");
  EXPECT_EQ(D1.ImplHash, getHashForStringList(
              {"struct", "S", "{", "int", "a", ";" , "}"}));
  checkRefs(D1.ImplRefs, {});

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "S_t");
  EXPECT_EQ(D2.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D2.DeclID, D1.DeclID);
  checkToRemove(D2, "typedef struct S {", "} S_t;\n");
  checkAddToClass(D2, "typedef struct S {", "} S_t;\n");
  checkRefs(D2.ImplRefs, {"S"});

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "");
  EXPECT_EQ(D3.Kind, Decl::Kind::Record);
  EXPECT_TRUE(D3.DeclID);
  EXPECT_TRUE(D3.IsUnnamed);
  checkToRemove(D3, "struct {", "} S2_t;\n");
  checkAddToClass(D3, "struct {", "} S2_t;\n");
  EXPECT_EQ(D3.ImplHash, getHashForStringList(
              {"struct", "{", "short", "b", ";", "}"}));
  checkRefs(D3.ImplRefs, {});

  const auto &D4 = getResult()[3];
  EXPECT_EQ(D4.Name, "S2_t");
  EXPECT_EQ(D4.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D4.DeclID, D3.DeclID);
  checkToRemove(D4, "typedef struct {", "} S2_t;\n");
  checkAddToClass(D4, "typedef struct {", "} S2_t;\n");
  checkRefs(D4.ImplRefs, {});

  const auto &D5 = getResult()[4];
  EXPECT_EQ(D5.Name, "S2_t2");
  EXPECT_EQ(D5.DeclID, nullptr);
  checkRefs(D5.ImplRefs, {"S2_t"});

  const auto &D6 = getResult()[5];
  EXPECT_EQ(D6.Name, "");
  EXPECT_EQ(D6.Kind, Decl::Kind::Record);
  EXPECT_TRUE(D6.DeclID);
  EXPECT_TRUE(D6.IsUnnamed);
  checkToRemove(D6, "struct {", "} Sa_t[][10];\n");
  checkAddToClass(D6, "struct {", "} Sa_t[][10];\n");
  EXPECT_EQ(D6.ImplHash, getHashForStringList(
              {"struct", "{", "int", "x", ";", "}"}));

  const auto &D7 = getResult()[6];
  EXPECT_EQ(D7.Name, "Sa_t");
  EXPECT_EQ(D7.Kind, Decl::Kind::Typedef);
  EXPECT_EQ(D7.DeclID, D6.DeclID);
  checkToRemove(D7, "typedef struct {", "} Sa_t[][10];\n");
  checkAddToClass(D7, "typedef struct {", "} Sa_t[][10];\n");
  checkRefs(D7.ImplRefs, {});
}


class EnumDeclTest : public MatcherTest {
protected:
  void SetUp() override {
    MatcherTest::SetUp();
    enableMatcher<EnumDecl>();
  }
};


static StringRef SimpleEnum = R"c(
enum E { E1, E2=2, E3 = E2 + 10};
)c";


TEST_F(EnumDeclTest, Simple) {
  ASSERT_TRUE(scanOnCode(SimpleEnum));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  EXPECT_EQ(D.Name, "E");
  EXPECT_EQ(D.Kind, Decl::Kind::Enum);
  checkToRemove(D, SimpleEnum.ltrim());
  checkAddToClass(D, SimpleEnum.ltrim());

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"enum", "E", "{", "E1", ",", "E2", "=", "2", ",", "E3", "=",
              "E2", "+", "10", "}"} ));

  EXPECT_TRUE(D.IsDefinition);
  EXPECT_FALSE(D.IsInline);
  EXPECT_FALSE(D.IsUnnamed);
  EXPECT_FALSE(D.IsUnion);
}


static StringRef EnumInLocalScope = R"c(
void func() {
  enum E { E1, E2=2, E3 = E2 + 10};
}
)c";


TEST_F(EnumDeclTest, InLocalScope) {
  ASSERT_TRUE(scanOnCode(EnumInLocalScope));
  // local enum should be ignored
  ASSERT_EQ(getResult().size(), 0);
}


static StringRef UnnamedEnum = R"c(
enum { E1, E2=2, E3 = E2 + 10};
)c";


TEST_F(EnumDeclTest, Unnamed) {
  ASSERT_TRUE(scanOnCode(UnnamedEnum));
  ASSERT_EQ(getResult().size(), 1);

  const auto &D = getResult().front();

  // Using the name of the first enumerator as the name of the enum.
  // Then it can be referred to on usage.
  EXPECT_TRUE(D.IsUnnamed);
  EXPECT_EQ(D.Name, "E1");

  EXPECT_EQ(D.Kind, Decl::Kind::Enum);
  checkToRemove(D, UnnamedEnum.ltrim());
  checkAddToClass(D, UnnamedEnum.ltrim());

  EXPECT_EQ(D.InfHash, hash_code(0));
  EXPECT_EQ(D.ImplHash, getHashForStringList(
              {"enum", "{", "E1", ",", "E2", "=", "2", ",", "E3", "=", "E2",
              "+", "10", "}"} ));
}


static StringRef EnumRelyOnTypesAndVars = R"c(
typedef short foo_t;
const int V = 1;
enum Named {EV};
enum {ENUM_VAL1 = 100, ENUM_VAL2 };
enum E { E1 = (foo_t)V, E2=ENUM_VAL2, E3 = E2 + 10, E4 = EV};
)c";


TEST_F(EnumDeclTest, RelyOnTypesAndVars) {
  ASSERT_TRUE(scanOnCode(EnumRelyOnTypesAndVars));
  ASSERT_EQ(getResult().size(), 3);

  const auto &D1 = getResult()[0];
  EXPECT_EQ(D1.Name, "Named");
  EXPECT_FALSE(D1.IsUnnamed);
  checkRefs(D1.ImplRefs, {});


  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "ENUM_VAL1");
  EXPECT_TRUE(D2.IsUnnamed);
  checkRefs(D2.ImplRefs, {});

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "E");
  EXPECT_FALSE(D1.IsUnnamed);

  // FIXME: 
  // Note: we record the EnumDecl, not the EnumDeclConstants.
  // "ENUM_VAL1" is a borrowed name for the unnamed enum
  checkRefs(D3.ImplRefs, {"ENUM_VAL1", "foo_t", "V", "Named"});
}


class VarDeclTest : public MatcherTest {
protected:
  void SetUp() override {
    MatcherTest::SetUp();
    enableMatcher<VarDecl>();
  }
};


static StringRef SimpleVar = R"c(
const int a;
static unsigned b;
void func() {
  float c;
  static double d;
  extern char e;
}
)c";


TEST_F(VarDeclTest, Simple) {
  ASSERT_TRUE(scanOnCode(SimpleVar));
  ASSERT_EQ(getResult().size(), 4);

  const auto &D1 = getResult()[0];
  EXPECT_EQ(D1.Name, "a");
  EXPECT_EQ(D1.Kind, Decl::Kind::Var);
  EXPECT_FALSE(D1.IsStatic);
  EXPECT_FALSE(D1.IsExtern);
  EXPECT_FALSE(D1.IsDefinition);
  checkToRemove(D1, "const int a;\n");
  checkAddToClass(D1, "const int a;\n");
  checkRefs(D1.ImplRefs, {});
  EXPECT_TRUE(D1.ImplHash == getHashForStringList({"const", "int", "a"}));

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "b");
  EXPECT_EQ(D2.Kind, Decl::Kind::Var);
  EXPECT_TRUE(D2.IsStatic);
  EXPECT_FALSE(D1.IsExtern);
  EXPECT_FALSE(D1.IsDefinition);
  checkToRemove(D2, "static unsigned b;\n");
  checkAddToClass(D2, "unsigned int b;\n");
  checkRefs(D2.ImplRefs, {});
  EXPECT_TRUE(D2.ImplHash == getHashForStringList({"static", "unsigned", "b"}));

  const auto &D3 = getResult()[2];
  EXPECT_EQ(D3.Name, "d");
  EXPECT_EQ(D3.Kind, Decl::Kind::Var);
  EXPECT_TRUE(D3.IsStatic);
  EXPECT_FALSE(D1.IsExtern);
  EXPECT_FALSE(D1.IsDefinition);
  checkToRemove(D3, "static double d;\n  ");
  checkAddToClass(D3, "double d;\n");
  checkRefs(D3.ImplRefs, {});
  EXPECT_TRUE(D3.ImplHash == getHashForStringList({"static", "double", "d"}));

  const auto &D4 = getResult()[3];
  EXPECT_EQ(D4.Name, "e");
  EXPECT_EQ(D4.Kind, Decl::Kind::Var);
  EXPECT_FALSE(D4.IsStatic);
  EXPECT_TRUE(D4.IsExtern);
  EXPECT_FALSE(D4.IsDefinition);
  checkToRemove(D4, "extern char e;\n");
  checkAddToClass(D4, "char e;\n");
  EXPECT_TRUE(D4.ImplHash == getHashForStringList({"extern", "char", "e"}));
};


static StringRef VarWithTypedefTypes = R"c(
typedef int foo_t;
foo_t a;
typedef struct S{ int x, y; } S_t;
S_t b = {1, 0};
)c";


TEST_F(VarDeclTest, WithTypedefTypes) {
  ASSERT_TRUE(scanOnCode(VarWithTypedefTypes));
  ASSERT_EQ(getResult().size(), 2);

  const auto &D1 = getResult()[0];
  EXPECT_EQ(D1.Name, "a");
  EXPECT_EQ(D1.Kind, Decl::Kind::Var);
  checkToRemove(D1, "foo_t a;\n");
  checkAddToClass(D1, "foo_t a;\n");
  checkRefs(D1.ImplRefs, {"foo_t"});
  EXPECT_TRUE(D1.ImplHash == getHashForStringList({"foo_t", "a"}));

  const auto &D2 = getResult()[1];
  EXPECT_EQ(D2.Name, "b");
  EXPECT_EQ(D2.Kind, Decl::Kind::Var);
  checkToRemove(D2, "S_t b = {1, 0};\n");
  checkAddToClass(D2, "S_t b = {1, 0};\n");
  checkRefs(D2.ImplRefs, {"S_t"});
  EXPECT_TRUE(D2.ImplHash == getHashForStringList(
              {"S_t", "b", "=", "{", "1", ",", "0", "}"}));
}


} // namespace clang::class_wrapper
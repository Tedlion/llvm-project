//===-- ClassWrapper.cpp - a tool to wrap code of with one single class ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// #include "clang/Tooling/CommonOptionsParser.h"

#include "../ClassWrapper.h"

#include "llvm/ADT/StringRef.h"
#include "clang/Tooling/JSONCompilationDatabase.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"
//#include "llvm/Support/WithColor.h"

#include <format>
using namespace clang;
using namespace clang::tooling;
using namespace llvm;

class PairParser : public llvm::cl::parser<std::pair<std::string, std::string>> {
public:
  using llvm::cl::parser<std::pair<std::string, std::string>>::parser;

  bool parse(llvm::cl::Option &O, llvm::StringRef ArgName,
             llvm::StringRef ArgValue, std::pair<std::string, std::string> &Val) {
    size_t EqualsPos = ArgValue.find('=');
    if (EqualsPos == llvm::StringRef::npos) {
      return O.error("Expected '=' in argument");
    }

    Val.first = ArgValue.substr(0, EqualsPos).str();
    Val.second = ArgValue.substr(EqualsPos + 1).str();
    return false;
  }

  // FIXME: Option help info require override implementation of the following functions

//  size_t getOptionWidth(const cl::Option &O) const override {
//    ;
//  }
//
//  void printOptionInfo(const cl::Option &O, size_t GlobalWidth) const override {
//
//  }
};

static cl::OptionCategory ClassWrapperCategory("Class Wrapper Options");
cl::list<std::string> InputFilename(cl::Positional, cl::ZeroOrMore, cl::desc("<input files>"),
                                    cl::cat(ClassWrapperCategory));

cl::list<std::pair<std::string, std::string>, bool, PairParser>
    OptCompilationDatabase(
        "p",
        cl::desc("compilation databases of one or more targets <compilation "
                 "database1> [compilation database2...]"),
        cl::value_desc("target:database"), cl::OneOrMore, cl::Required,
        cl::CommaSeparated, cl::cat(ClassWrapperCategory));

cl::opt<std::string> OutputDir("o", cl::desc("output dictionary"),
                               cl::value_desc("out_dir"),
                               cl::Required,
                               cl::cat(ClassWrapperCategory));

// We need SourceRoot to distinguish user symbol declarations and system
// declarations.
cl::opt<std::string> SourceRoot("r", cl::desc("user sources root"),
                                cl::value_desc("src_root"), cl::Required,
                                cl::cat(ClassWrapperCategory));

cl::list<std::string> NonWrappedFiles(
    "e",
    cl::desc(
        "function/type declarations in specific files will not be wrapped"),
    cl::value_desc("exclude_files"), cl::ZeroOrMore,
    cl::cat(ClassWrapperCategory));

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  if (!cl::ParseCommandLineOptions(argc, argv)) {
    return 1;
  }

  llvm::outs() << "Input files: ";
  for (const auto & Filename: InputFilename) {
    llvm::outs() << Filename << " ";
  }
  llvm::outs() << "\n\nCompilation Databases: \n";

  for (const auto & [Target, DatabasePath]: OptCompilationDatabase) {
    llvm::outs() << std::format("{}:{}\n", Target, DatabasePath);
    std::string ErrorMessage;
        auto Database = JSONCompilationDatabase::loadFromFile(
        DatabasePath, ErrorMessage, JSONCommandLineSyntax::AutoDetect);
        if (!Database) {
          llvm::errs() << ErrorMessage << "\n";
          return 1;
        }
  }



  return 0;
}
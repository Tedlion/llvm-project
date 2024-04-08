//===-- ClassWrapper.cpp - a tool to wrap code of with one single class ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// #include "clang/Tooling/CommonOptionsParser.h"

#include "../ClassWrapperContext.h"
#include "../DeclScanner.h"
#include "../FileFilter.h"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"

#include <format>
using namespace clang;
using namespace clang::tooling;
using namespace clang::class_wrapper;
using namespace llvm;

class PairParser
    : public llvm::cl::parser<std::pair<std::string, std::string>> {
public:
  using llvm::cl::parser<std::pair<std::string, std::string>>::parser;

  bool parse(llvm::cl::Option &O, llvm::StringRef ArgName,
             llvm::StringRef ArgValue,
             std::pair<std::string, std::string> &Val) {
    size_t EqualsPos = ArgValue.find('=');
    if (EqualsPos == llvm::StringRef::npos) {
      return O.error("Expected '=' in argument");
    }

    Val.first = ArgValue.substr(0, EqualsPos).str();
    Val.second = ArgValue.substr(EqualsPos + 1).str();
    return false;
  }

  // FIXME: Option help info require override implementation of the following
  // functions

  //  size_t getOptionWidth(const cl::Option &O) const override {
  //    ;
  //  }
  //
  //  void printOptionInfo(const cl::Option &O, size_t GlobalWidth) const
  //  override {
  //
  //  }
};

static cl::OptionCategory ClassWrapperCategory("Class Wrapper Options");

// We need SourceRoot to distinguish user symbol declarations and system
// declarations.
cl::opt<std::string> SourceRoot(cl::Positional, cl::Required,
                                cl::value_desc("User sources root"),
                                cl::desc("<src root>"),
                                cl::cat(ClassWrapperCategory));

cl::list<std::string> FilenameFilters(
    "f", cl::ZeroOrMore, cl::CommaSeparated, cl::value_desc("files_filters"),
    cl::desc("File filter rules.\n"
             "Multiple file paths with wildcard characters are accepted.\n"
             "File paths may be absolute or relative to the source root.\n"
             "If a file path starts with '-', matched files will be excluded.\n"
             "The behind rules override the front ones."),
    cl::cat(ClassWrapperCategory));

cl::list<std::pair<std::string, std::string>, bool, PairParser>
    OptCompilationDatabase(
        "p",
        cl::desc("Compilation databases of one or more targets.\n"
                 "e.g. <target1=database1> [target2=database2...]"),
        cl::value_desc("target:database"), cl::OneOrMore, cl::Required,
        cl::CommaSeparated, cl::cat(ClassWrapperCategory));

cl::opt<std::string> OutputDir("o", cl::desc("Output dictionary"),
                               cl::value_desc("out_dir"), cl::Required,
                               cl::cat(ClassWrapperCategory));

cl::list<std::string> NonWrappedFiles(
    "non-wrapped",
    cl::desc("Function/type declarations in given files will not be wrapped.\n"
             "Differently from -f, non-wrapped files will be copied unchanged "
             "instead of being ignored."),
    cl::value_desc("exclude_files"), cl::ZeroOrMore,
    cl::cat(ClassWrapperCategory));

cl::list<std::string>
    ExtraArgs("extra-arg",
              cl::desc("Additional arguments to append to the "
                       "compilation command line."),
              cl::value_desc("extra_args"), cl::ZeroOrMore, cl::CommaSeparated,
              cl::cat(ClassWrapperCategory));

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  cl::HideUnrelatedOptions(ClassWrapperCategory);

  if (!cl::ParseCommandLineOptions(argc, argv)) {
    return 1;
  }

  FileFilter SrcFilter(FilenameFilters.begin(), FilenameFilters.end(),
                       SourceRoot);
  FileFilter NonWrappedFilter(NonWrappedFiles.begin(), NonWrappedFiles.end(),
                              SourceRoot);
  ClassWrapperContext Context;
  llvm::outs() << "\n\nCompilation Databases: \n";

  for (const auto &[Target, DatabasePath] : OptCompilationDatabase) {
    llvm::outs() << std::format("{}:{}\n", Target, DatabasePath);
    std::string ErrorMessage;
    auto Database = std::make_unique<ArgumentsAdjustingCompilations>(
        CompilationDatabase::loadFromDirectory(DatabasePath, ErrorMessage));
    for (const auto &Arg : ExtraArgs) {
      Database->appendArgumentsAdjuster(getInsertArgumentAdjuster(Arg.data()));
    }

    if (!Database) {
      llvm::errs() << ErrorMessage << "\n";
      return 1;
    }

    std::vector<std::string> ScanningFiles;
    runDeclScanner(*Database, ScanningFiles, Context);

    for (auto Filepath : Database->getAllFiles()) {
      if (SrcFilter.isMatched(Filepath) &&
          !NonWrappedFilter.isMatched(Filepath)) {
        ScanningFiles.push_back(Filepath);
        llvm::outs() << Filepath << "\n";
      }
    }

    llvm::outs() << "\n";
  }

  return 0;
}
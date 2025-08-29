//===-- ClassWrapper.cpp - a tool to wrap code of with one single class ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../ClassWrapperContext.h"
#include "../DeclScanner.h"
#include "../FileFilter.h"
#include "../Support.h"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Parallel.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ThreadPool.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <thread>


using namespace clang;
using namespace clang::class_wrapper;
using namespace clang::tooling;
using namespace llvm;


namespace {
class PairParser
    : public cl::basic_parser<std::pair<std::string, std::string>> {
public:
  using cl::basic_parser<std::pair<std::string, std::string> >::basic_parser;


  bool parse(cl::Option &O, StringRef ArgName, StringRef ArgValue,
             std::pair<std::string, std::string> &Val) {
    size_t EqualsPos = ArgValue.find('=');
    if (EqualsPos == llvm::StringRef::npos) {
      return O.error("Expected '=' in argument");
    }

    Val.first = ArgValue.substr(0, EqualsPos).str();
    Val.second = ArgValue.substr(EqualsPos + 1).str();
    return false;
  }

  StringRef getValueName() const override {
    return "key=value";
  }
};

cl::OptionCategory ClassWrapperCategory("Class Wrapper Options");

// We need SourceRoot to distinguish user symbol declarations and system
// declarations.
cl::opt<std::string> SourceRoot(cl::Positional, cl::Required,
                                cl::desc("<src_root>"),
                                cl::cat(ClassWrapperCategory));

cl::opt<std::string> OutputDir(cl::Positional, cl::Required,
                               cl::desc("<out_dir>"),
                               cl::cat(ClassWrapperCategory));

cl::list<std::string> FilenameFilters(
    "f", cl::OneOrMore, cl::CommaSeparated, cl::value_desc("files_filters"),
    cl::desc("File filter rules.\n"
        "Multiple file paths with wildcard characters are accepted.\n"
        "File paths may be absolute or relative to the source root.\n"
        "If a file path starts with '-', matched files will be excluded.\n"
        "The behind rules override the front ones."),
    cl::cat(ClassWrapperCategory));

cl::list<std::pair<std::string, std::string>, bool, PairParser>
OptCompilationDatabase(
    "p", cl::OneOrMore, cl::CommaSeparated, cl::value_desc("target=database"),
    cl::desc("Compilation databases of one or more targets.\n"
        "e.g. <target1=database1>[,target2=database2...]"),
    cl::cat(ClassWrapperCategory));

cl::list<std::string> NonWrappedFiles(
    "non-wrapped", cl::ZeroOrMore, cl::CommaSeparated,
    cl::value_desc("files"),
    cl::desc("Function/type declarations in given files will NOT be "
             "wrapped in class or namespace,\n"
             " and macros will Not be expanded.\n"
             "Differently from -f, non-wrapped files will be copied unchanged "
             "instead of being ignored."),
    cl::cat(ClassWrapperCategory));

cl::list<std::string> ExtraArgs(
    "extra-arg", cl::ZeroOrMore, cl::CommaSeparated,
    cl::value_desc("args"),
    cl::desc("Additional compilation arguments to append"),
    cl::cat(ClassWrapperCategory));

cl::opt<bool> ReScan("r", cl::init(false),
    cl::desc("Re-scan all the source files.\n"
             "Otherwise, only out-of-date files will be scanned."),
             cl::cat(ClassWrapperCategory));

cl::opt<unsigned> Parallel(
    "j", cl::init(1),
    cl::desc("Parallel jobs count, default: 1\n"),
    cl::cat(ClassWrapperCategory));
} // namespace


static bool requireRescan(StringRef Target, StringRef RelativePath,
                          const ClassWrapperContext &Context) {
  std::string DependencyPath = Context.getDependencyPath(Target, RelativePath);
  std::string ScanResultPath = Context.getScanResultPath(Target, RelativePath);

  if (needUpdate(DependencyPath, ScanResultPath) ||
      needUpdate(Target, DependencyPath))
    return true;

  std::vector<std::string> Dependencies;
  std::ifstream DependencyFile(DependencyPath);
  std::string Line;

  while (std::getline(DependencyFile, Line)) {
    StringRef Trimmed = StringRef(Line).trim();
    if (Trimmed.empty())
      continue;
    Dependencies.push_back(Trimmed.str());
  }

  return needUpdate(Target, Dependencies);
}


int main(int argc, const char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);
  cl::HideUnrelatedOptions(ClassWrapperCategory);

  if (!cl::ParseCommandLineOptions(argc, argv))
    return 1;

  FileFilter SrcFilter(FilenameFilters.begin(), FilenameFilters.end(),
                       SourceRoot);
  FileFilter NonWrappedFilter(NonWrappedFiles.begin(), NonWrappedFiles.end(),
                              SourceRoot);

  if (Parallel == 0) {
    parallel::strategy.ThreadsRequested = std::thread::hardware_concurrency();
  } else {
    parallel::strategy.ThreadsRequested =
        std::min(Parallel.getValue(), std::thread::hardware_concurrency());
  }

  ClassWrapperContext Context(SourceRoot, OutputDir, SrcFilter,
                              NonWrappedFilter);

  parallel::TaskGroup Tasks;

  for (const auto &[Target, DatabasePath] : OptCompilationDatabase) {
    llvm::outs() << std::format("Scan target {}({}):\n", Target, DatabasePath);
    std::string ErrorMessage;

    auto Compilations = CompilationDatabase::loadFromDirectory(
        DatabasePath, ErrorMessage);

    if (!Compilations) {
      llvm::errs() << ErrorMessage << "\n";
      return 1;
    }

    auto AdjustingCompilations = std::make_shared<
      ArgumentsAdjustingCompilations>(std::move(Compilations));

    for (const auto &Arg : ExtraArgs) {
      AdjustingCompilations->appendArgumentsAdjuster(
          getInsertArgumentAdjuster(Arg.data()));
    }

    AdjustingCompilations->appendArgumentsAdjuster(
        getInsertArgumentAdjuster("-w"));
    AdjustingCompilations->appendArgumentsAdjuster(
        getInsertArgumentAdjuster("-Wno-error"));
    AdjustingCompilations->appendArgumentsAdjuster(
        getInsertArgumentAdjuster("-fparse-all-comments"));
    for (auto & Filename : AdjustingCompilations->getAllFiles()) {
      if (!SrcFilter.isMatched(Filename))
        continue;

      std::string RelativePath = Context.getRelativePath(Filename);
      if (!ReScan && !requireRescan(Target, RelativePath, Context)) {
        llvm::outs() << std::format("  Skip: {}\n", RelativePath);
        continue;
      }

      Tasks.spawn([Target, Filename, AdjustingCompilations,  &Context] {
        DeclScanner::run(Target, Filename, *AdjustingCompilations, Context);
      });
    }

    // Context.setScanningTarget(Target);
    llvm::outs() << "\n";
  }

  Tasks.sync();


  return 0;
}
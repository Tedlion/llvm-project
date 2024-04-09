/**
 * @brief
 * @authors tangwy
 * @date 2024/4/8
 */

#ifndef CLASSWRAPPERCONTEXT_H
#define CLASSWRAPPERCONTEXT_H

#include "FileFilter.h"

#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/Specifiers.h"
#include "clang/Tooling/Core/Replacement.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"

#include <map>
#include <set>


namespace clang::class_wrapper {

using llvm::StringRef;
using llvm::StringMap;
using llvm::StringSet;
using tooling::Replacement;
using tooling::Replacements;

// Data structure to store the scanning result?
// How to use the result?
// To decide replacement:
// To apply replacement: vectors?


struct SymbolInfoKey {
  StringRef FileName;
  unsigned BeginOffset;
  unsigned EndOffset;
//  SourceRange Loc;

  auto operator<=>(const SymbolInfoKey &) const = default;
};

struct SymbolInfoValue{
  StringRef Target;

  // Attributes from AST
  StorageClass Storage;
  bool IsInline;
  bool IsFuncPtr;
  // hash value

  // Decision made
  std::string NewName; // use old name if empty

};

struct ClassWrapperContext {
  const std::string SourceRoot;
  const llvm::FileFilter &SrcFilter;
  const llvm::FileFilter &NonWrappedFilter;

  StringMap<std::map<SymbolInfoKey, SmallVector<SymbolInfoValue, 4>>>
      DeclSymbols;
  StringSet<> UsedSymbolName;

  Replacements Replaces;

  ClassWrapperContext(const std::string & SourceRoot, const llvm::FileFilter &SrcFilter,
                      const llvm::FileFilter &NonWrappedFilter)
      : SourceRoot(llvm::pathNormalize(SourceRoot)), SrcFilter(SrcFilter),
        NonWrappedFilter(NonWrappedFilter) {}

  bool needToWrap(StringRef FilePath) const {
    std::string NormalFilePath = llvm::pathNormalize(FilePath.str());

    if (NormalFilePath.find(SourceRoot) != 0){
      return false;
    }

    return !NonWrappedFilter.isMatched(NormalFilePath);
  }

};
}; // namespace clang::class_wrapper

#endif // CLASSWRAPPERCONTEXT_H

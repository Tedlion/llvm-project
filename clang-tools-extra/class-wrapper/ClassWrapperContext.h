/**
 * @brief
 * @authors tangwy
 * @date 2024/4/8
 */

#ifndef CLASSWRAPPERCONTEXT_H
#define CLASSWRAPPERCONTEXT_H

#include "FileFilter.h"

#include "clang/AST/Decl.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/Specifiers.h"
#include "clang/Tooling/Core/Replacement.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/VirtualFileSystem.h"

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
  Decl::Kind Kind;
  StorageClass Storage;
  unsigned IsInline   :1;
  unsigned IsFuncPtr  :1;
  unsigned IsDefinition : 1; // Note: for variable, it means whether it is
                             // initialized (i.e., is strong defined)

  // hash value
  unsigned DeclTypeHash;
  unsigned ImplHash;  // Function Define hash or Variable Init hash

  // Decision made
  std::string NewName; // use old name if empty

};

class ClassWrapperContext {
public:
  ClassWrapperContext(const std::string &SourceRoot,
                      const llvm::FileFilter &SrcFilter,
                      const llvm::FileFilter &NonWrappedFilter,
                      IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS,
                      IntrusiveRefCntPtr<FileManager> Files)
      : SourceRoot(llvm::pathNormalize(SourceRoot)), SrcFilter(SrcFilter),
        NonWrappedFilter(NonWrappedFilter), BaseFS(std::move(FS)),
        Files(std::move(Files)) {}

  const std::string SourceRoot;
  const llvm::FileFilter &SrcFilter;
  const llvm::FileFilter &NonWrappedFilter;

  bool needToWrap(StringRef FilePath) const {
    std::string NormalFilePath = llvm::pathNormalize(FilePath.str());

    if (NormalFilePath.find(SourceRoot) != 0){
      return false;
    }

    return !NonWrappedFilter.isMatched(NormalFilePath);
  }

  StringRef getScanningTarget() const { return ScanningTarget; }

  void setScanningTarget(StringRef Target) { ScanningTarget = Target; }

  void recordSymbol(StringRef Name, SourceRange Range, Decl::Kind Kind,
                    StorageClass Storage,
                    unsigned DeclTypeHash, std::optional<unsigned> ImplHash,
                    bool IsInline = false,
                    bool IsFuncPtr = false);

  IntrusiveRefCntPtr<llvm::vfs::FileSystem> getBaseFS() const { return BaseFS; }
  IntrusiveRefCntPtr<FileManager> getFiles() const { return Files; }

private:
  StringRef ScanningTarget;

  StringMap<std::map<SymbolInfoKey, SmallVector<SymbolInfoValue, 4>>>
      DeclSymbols;
  StringSet<> UsedSymbolName;

  Replacements Replaces;

  IntrusiveRefCntPtr<llvm::vfs::FileSystem> BaseFS;
  IntrusiveRefCntPtr<FileManager> Files;
};
}; // namespace clang::class_wrapper

#endif // CLASSWRAPPERCONTEXT_H

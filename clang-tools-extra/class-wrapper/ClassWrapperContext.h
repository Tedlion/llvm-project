/**
 * @brief
 * @authors tangwy
 * @date 2024/4/8
 */

#ifndef CLASSWRAPPERCONTEXT_H
#define CLASSWRAPPERCONTEXT_H

#include "FileFilter.h"
#include "ExtendedODRHash.h"

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

/// After scanning the source code once, to-decide replacements are stored.
///

#if 0
struct SymbolInfoKey {
  StringRef FileName;
  unsigned BeginOffset;
  unsigned EndOffset;
//  SourceRange Loc;

  auto operator<=>(const SymbolInfoKey &) const = default;
};
#endif

class RefEntry {
  friend class DeclEntry;
  std::string Name;
  Decl::Kind Kind;
  StorageClass Storage = SC_Extern;

  // points to the SameRange of DeclEntry, only set when UsedAsFunctionPtr
  tooling::Range Range;

  bool UsedAsFunctionPtr = false;
};


class DeclEntry {
  std::string Name;
  std::string FilePath; // source file's path relative to SourceRoot
  Decl::Kind Kind;
  StorageClass Storage;

  std::string Expansion;

  // If the Expansion is empty, the following Ranges points to the sources;
  // otherwise, the Ranges points to the Expansion.
  tooling::Range NameRange;
  tooling::Range InfRange;
  tooling::Range ImplRange;

  llvm::hash_code InfHash;
  llvm::hash_code ImplHash;

  bool IsDefinition;
  bool IsInline = false;
  SmallVector<RefEntry, 4> InfRefs;  // the symbols used in the declaration
  SmallVector<RefEntry, 4> ImplRefs; // the symbols used in the definition
};

struct SymbolInfo {
  StringRef Target;

  // Attributes from AST
  Decl::Kind Kind;
  StorageClass Storage;
  unsigned IsInline   :1;
  unsigned IsFuncPtr  :1;
  unsigned IsDefinition : 1; // Note: for variable, it means whether it is
                             // initialized (i.e., is strong defined)

  // hash value
  ExtendedODRHash::HashValue DeclTypeHash;
  ExtendedODRHash::HashValue  ImplHash;  // Function Define hash or Variable Init hash

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

  void recordSymbol(const SymbolRecordEntry &Entry);

  IntrusiveRefCntPtr<llvm::vfs::FileSystem> getBaseFS() const { return BaseFS; }
  IntrusiveRefCntPtr<FileManager> getFiles() const { return Files; }

private:
  StringRef ScanningTarget;
  using RecordRange = std::pair<std::string, tooling::Range>;

  struct ReplacementRangeComparator {
    bool operator()(const RecordRange &Lhs, const RecordRange &Rhs) const {
      if (Lhs.first != Rhs.first) {
        return Lhs.first < Rhs.first;
      }
      return Lhs.second.getOffset() < Rhs.second.getOffset();
    }
  };

  StringMap<
      std::map<RecordRange, SmallVector<SymbolInfo, 4>, ReplacementRangeComparator>>
      DeclSymbols;
  StringSet<> UsedSymbolName;

  Replacements Replaces;

  IntrusiveRefCntPtr<llvm::vfs::FileSystem> BaseFS;
  IntrusiveRefCntPtr<FileManager> Files;
};
}; // namespace clang::class_wrapper

#endif // CLASSWRAPPERCONTEXT_H

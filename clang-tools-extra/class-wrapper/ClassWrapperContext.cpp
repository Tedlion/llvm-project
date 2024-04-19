/**
 * @brief
 * @authors tangwy
 * @date 2024/4/17
 */

#include "ClassWrapperContext.h"

namespace clang::class_wrapper {

void ClassWrapperContext::recordSymbol(StringRef Name, CharSourceRange Range,
                                       Decl::Kind Kind, StorageClass Storage,
                                       unsigned int DeclTypeHash,
                                       std::optional<unsigned int> ImplHash,
                                       bool IsInline, bool IsFuncPtr) {
  // TODO:
  SymbolInfo Info{Name, Kind, Storage, IsInline, IsFuncPtr, ImplHash.has_value(),
                  DeclTypeHash, ImplHash.value_or(0)};
  DeclSymbols[Name][Range].push_back(Info);
}
}; // namespace clang::class_wrapper

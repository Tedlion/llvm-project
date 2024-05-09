/**
 * @brief
 * @authors tangwy
 * @date 2024/4/17
 */

#include "ClassWrapperContext.h"

namespace clang::class_wrapper {

void ClassWrapperContext::recordSymbol(const SymbolRecordEntry &Entry) {
  // TODO:
  SymbolInfo Info{Entry.Name,      Entry.Kind,
                  Entry.Storage,   Entry.IsInline,
                  Entry.IsFuncPtr, static_cast<bool>(Entry.ImplHash.Valid),
                  Entry.InfHash,   Entry.ImplHash};
  DeclSymbols[Entry.Name][std::make_pair(Entry.FilePath, Entry.CharRange)]
      .push_back(Info);
}
} // namespace clang::class_wrapper

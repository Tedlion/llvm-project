/**
 * @brief
 * @authors tangwy
 * @date 2024/4/17
 */

#include "ClassWrapperContext.h"

namespace clang::class_wrapper {

void ClassWrapperContext::recordSymbol(StringRef Name, SourceRange Range,
                                       Decl::Kind Kind, StorageClass Storage,
                                       unsigned int DeclTypeHash,
                                       std::optional<unsigned int> ImplHash,
                                       bool IsInline, bool IsFuncPtr) {
  // TODO:


}
}; // namespace clang::class_wrapper

/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#ifndef DECLSCANNER_H
#define DECLSCANNER_H

#include "ClassWrapperContext.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "llvm/ADT/ArrayRef.h"

namespace clang::class_wrapper {
using namespace clang::tooling;
using namespace llvm;
extern void runDeclScanner(const CompilationDatabase &Compilations,
                           const ArrayRef<std::string> SourcePaths,
                           ClassWrapperContext &Context);

} // namespace clang::class_wrapper

#endif // DECLSCANNER_H

/**
 * @brief
 * @authors tangwy
 * @date 2024/3/27
 */

#include "DeclScanner.h"
#include "clang/Tooling/Tooling.h"
namespace clang::class_wrapper {
void runDeclScanner(const CompilationDatabase &Compilations,
                    const llvm::ArrayRef<std::string> SourcePaths,
                    ClassWrapperContext &Context) {
  ClangTool Tool(Compilations, SourcePaths);
  // TODO:
}

} // namespace clang::class_wrapper

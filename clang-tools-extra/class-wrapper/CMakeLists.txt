set(LLVM_LINK_COMPONENTS
        support
        FrontendOpenMP
)

#add_clang_library(classWrapper
#
#        DEPENDS
#        omp_gen
#)
#

#clang_target_link_libraries(classWrapper
#        PRIVATE
#        clangAST
#        clangASTMatchers
#        clangBasic
#        clangDynamicASTMatchers
#        clangFrontend
#        clangTooling
#        clangSerialization
#)

add_subdirectory(tool)

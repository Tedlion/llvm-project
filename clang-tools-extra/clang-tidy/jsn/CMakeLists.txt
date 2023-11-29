set(CMAKE_CXX_STANDARD 20)

set(LLVM_LINK_COMPONENTS
        FrontendOpenMP
        Support
)

add_clang_library(clangTidyJsnModule
        JsnTidyModule.cpp

        DeprecatedTimerCheck.cpp

        LINK_LIBS
        clangTidy
        clangTidyUtils

        DEPENDS
        omp_gen
)

clang_target_link_libraries(clangTidyJsnModule
        PRIVATE
        clangAST
        clangASTMatchers
        clangBasic
        clangLex
)

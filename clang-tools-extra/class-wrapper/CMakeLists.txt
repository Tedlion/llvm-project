set(CMAKE_CXX_STANDARD 23)

set(LLVM_LINK_COMPONENTS
        support
        FrontendOpenMP
)

add_definitions(-D_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING)

add_clang_library(classWrapper
        DeclScanner.cpp
        FileFilter.cpp
        ClassWrapperContext.cpp
        ExtendedODRHash.cpp
)

clang_target_link_libraries(classWrapper
        PRIVATE
        clangAnalysis
        clangAST
        clangASTMatchers
        clangBasic
        clangFormat
        clangFrontend
        clangLex
        clangSerialization
        clangTooling
        clangToolingCore
        clangTransformer
)

add_subdirectory(tool)

if(CLANG_INCLUDE_TESTS)
    add_subdirectory(unittests)
endif()

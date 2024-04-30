include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

set(LLVM_LINK_COMPONENTS
        support
        AllTargetsInfos
        FrontendOpenMP
)

add_custom_target(ClassWrapperUnitTests)

add_unittest(ClassWrapperUnitTests ClassWrapperTests
        DeclScannerTests.cpp)


clang_target_link_libraries(ClassWrapperTests
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
)
target_link_libraries(ClassWrapperTests
        PRIVATE
        LLVMTestingAnnotations
        LLVMTestingSupport
)

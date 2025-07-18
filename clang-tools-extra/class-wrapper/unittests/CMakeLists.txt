include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

set(LLVM_LINK_COMPONENTS
        support
        AllTargetsInfos
        FrontendOpenMP
)

add_custom_target(ClassWrapperUnitTests)

add_unittest(ClassWrapperUnitTests ClassWrapperTests
        DeclScannerTests.cpp
        FileFilterTests.cpp)

if(MSVC)
    target_compile_options(classWrapper PRIVATE /Od /Zi)
else()
    target_compile_options(classWrapper PRIVATE -O0 -g)
endif()

target_compile_definitions(ClassWrapperTests PRIVATE _SILENCE_CXX23_ALIGNED_UNION_DEPRECATION_WARNING)

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
        classWrapper
        LLVMTestingAnnotations
        LLVMTestingSupport
#        clangAST
#        clangASTMatchers
#        clangBasic
#        clangFormat
#        clangFrontend
#        clangRewrite
#        clangSerialization
#        clangTooling
#        clangToolingCore
)

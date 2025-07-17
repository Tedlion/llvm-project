include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

add_clang_tool(class-wrapper
        ClassWrapperMain.cpp
)

if(MSVC)
    target_compile_options(class-wrapper PRIVATE /Od /Zi)
else()
    target_compile_options(class-wrapper PRIVATE -O0 -g)
endif()

target_compile_definitions(class-wrapper PRIVATE _SILENCE_CXX23_ALIGNED_UNION_DEPRECATION_WARNING)


clang_target_link_libraries(class-wrapper
        PRIVATE
        clangAST
        clangASTMatchers
        clangBasic
        clangDynamicASTMatchers
        clangFrontend
        clangSerialization
        clangTooling
        clangTransformer
)

target_link_libraries(class-wrapper
        PRIVATE
        classWrapper
)

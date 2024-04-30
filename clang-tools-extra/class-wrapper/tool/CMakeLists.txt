include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

add_clang_tool(class-wrapper
        ClassWrapperMain.cpp
)

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

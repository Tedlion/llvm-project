include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

add_clang_tool(class-wrapper
        ClassWrapperMain.cpp
        ../DeclScanner.cpp
        ../FileFilter.cpp
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
)

#target_link_libraries(clang-query
#        PRIVATE
#        clangQuery
#)

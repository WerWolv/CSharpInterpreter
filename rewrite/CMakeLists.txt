cmake_minimum_required(VERSION 3.15)
project(rewrite)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(lib/third_party/fmt)
add_subdirectory(lib/third_party/libwolv)
add_subdirectory(lib/third_party/magic_enum)

add_executable(CSharpInterpreter_Rewrite
    source/main.cpp
    source/assembly.cpp
    source/data_structures.cpp
    source/runtime.cpp
    source/execution.cpp
)
target_include_directories(CSharpInterpreter_Rewrite PRIVATE include)
target_compile_options(CSharpInterpreter_Rewrite PRIVATE -Wall -Wextra -Wpedantic -Wno-deprecated-declarations -Werror)
target_link_libraries(CSharpInterpreter_Rewrite PRIVATE fmt::fmt wolv::types wolv::io magic_enum::magic_enum)
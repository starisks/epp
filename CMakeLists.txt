cmake_minimum_required(VERSION 3.20)
project(epp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(epp
  src/main.cpp
  src/source.cpp
  src/lexer.cpp
  src/parser.cpp
  src/ast.cpp
  src/runtime.cpp
)

target_include_directories(epp PRIVATE src)

if (MSVC)
  target_compile_options(epp PRIVATE /W4)
else()
  target_compile_options(epp PRIVATE -Wall -Wextra -Wpedantic)
endif()

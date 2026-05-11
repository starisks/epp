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

find_package(CURL REQUIRED)
target_link_libraries(epp PRIVATE CURL::libcurl)

if (MSVC)
  target_compile_options(epp PRIVATE /W4)
else()
  target_compile_options(epp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (MINGW)

  # Ensure correct Windows binary behavior
  target_compile_definitions(epp PRIVATE _WIN32_WINNT=0x0A00)

  # 🔥 FIX for missing runtime DLL dependency (SAFE version)
  target_link_options(epp PRIVATE
    -static-libgcc
    -static-libstdc++
  )

endif()
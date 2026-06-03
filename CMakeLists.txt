cmake_minimum_required(VERSION 3.30)

project(engine
    VERSION 0.0.1
    DESCRIPTION "Bevy-inspired C++ game engine"
    LANGUAGES CXX
)

# Through M4 we target Fedora-only with GCC 16 / Clang 20 (see ADR 0002).
# Both compilers ship enough C++26 for our needs; we still allow per-file fallback
# to C++23 via target_compile_features when a specific feature isn't ready.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:preprocessor /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

option(ENGINE_BUILD_TESTS    "Build unit tests"   ON)
option(ENGINE_BUILD_EXAMPLES "Build examples"     ON)
option(ENGINE_UNITY_BUILD    "Unity build engine" ON)
option(ENGINE_ENABLE_ASAN    "AddressSanitizer"   OFF)
option(ENGINE_ENABLE_UBSAN   "UndefinedBehaviorSanitizer" OFF)

if(ENGINE_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(ENGINE_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

add_subdirectory(engine)

if(ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ENGINE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

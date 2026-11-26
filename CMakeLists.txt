cmake_minimum_required(VERSION 3.20)
project(NexaMachine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(nexa
    src/main.cpp
    src/vm.cpp
)

target_include_directories(nexa PRIVATE include)

if(MSVC)
    target_compile_options(nexa PRIVATE /W4 /permissive-)
else()
    target_compile_options(nexa PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(nexa_tests tests/assembler_tests.cpp src/assembler.cpp)
    target_include_directories(nexa_tests PRIVATE include)
    add_test(NAME nexa_assembler_tests COMMAND nexa_tests)
endif()

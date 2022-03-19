cmake_minimum_required(VERSION 3.21)
project(neptun)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
        expected
        URL https://github.com/TartanLlama/expected/archive/refs/tags/v1.0.0.zip
)
FetchContent_MakeAvailable(expected)

include_directories(.)

enable_testing()

add_subdirectory(common)
add_subdirectory(network)
add_subdirectory(neptun)

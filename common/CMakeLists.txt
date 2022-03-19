include_directories(.)

add_library(lib_common types.h flip_buffer.h testing.h errors.h)
target_link_libraries(lib_common LINK_PUBLIC expected)
set_target_properties(lib_common PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(lib_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Tests

include(FetchContent)

add_executable(
        common_tests types_test.cc)

target_link_libraries(
        common_tests
        lib_common
        gmock
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(common_tests)
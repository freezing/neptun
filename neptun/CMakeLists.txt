include_directories(.)

add_library(lib_neptun neptun.h)
set_target_properties(lib_neptun PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(lib_neptun LINK_PUBLIC lib_network)
target_include_directories(lib_neptun PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Example
add_executable(bin_neptun example.cc)
target_link_libraries(bin_neptun lib_neptun)


# Tests

include(FetchContent)

add_executable(
        neptun_tests neptun_test.cc)

target_link_libraries(
        neptun_tests
        lib_neptun
        gmock
        gtest_main
)

include(GoogleTest)
#gtest_discover_tests(neptun_tests)
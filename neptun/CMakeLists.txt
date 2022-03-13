include_directories(.)

add_library(lib_neptun)
set_target_properties(lib_neptun PROPERTIES LINKER_LANGUAGE CXX)

# Tests

include(FetchContent)

add_executable(
        neptun_tests)

target_link_libraries(
        neptun_tests
        lib_neptun
        gmock
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(neptun_tests)
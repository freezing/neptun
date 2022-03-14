# Example: https://cmake.org/examples/
# Google test and CMake: https://google.github.io/googletest/quickstart-cmake.html

include_directories(.)

add_library(lib_network
        network.h udp_socket.h fake_network.h ip_address.h io_buffer.h message.h)
target_link_libraries(lib_network LINK_PUBLIC lib_common)
set_target_properties(lib_network PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(lib_network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Tests

include(FetchContent)

add_executable(
        network_tests
        ip_address_test.cc
        fake_network_test.cc
        udp_socket_test.cc testing_helpers.h io_buffer_test.cc message_test.cc)

target_link_libraries(
        network_tests
        lib_network
        gmock
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(network_tests)
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

include_directories(.)

add_executable(neptun
        main.cc
        ring_buffer.h
        network.h udp_socket.h fake_network.h ip_address.h)



# Tests
enable_testing()

add_executable(
        tests
        ip_address_test.cc
        fake_network_test.cc
        udp_socket_test.cc testing_helpers.h)
target_link_libraries(
        tests
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(tests)
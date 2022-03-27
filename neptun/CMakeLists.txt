include_directories(.)

add_library(lib_neptun neptun.h messages/packet_header.h messages/message_header.h messages/segment.h reliable_stream.h common.h packet_delivery_manager.h messages/reliable_message.h error.h unreliable_stream.h neptun_metrics.h connection_manager.h)
target_link_libraries(lib_neptun LINK_PUBLIC lib_common lib_network expected)
set_target_properties(lib_neptun PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(lib_neptun PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Example
add_executable(bin_neptun example.cc)
target_link_libraries(bin_neptun lib_neptun)

add_executable(bin_udp example_udp.cc)
target_link_libraries(bin_udp lib_neptun)

add_executable(pcap pcap/main.cc pcap/file_format.h)
target_link_libraries(pcap LINK_PUBLIC lib_neptun lib_common expected)

# Tests

include(FetchContent)

add_executable(
        neptun_tests neptun_test.cc messages/packet_header.cc reliable_stream_test.cc packet_delivery_manager_test.cc connection_manager_test.cc)

target_link_libraries(
        neptun_tests
        lib_neptun
        lib_common
        gmock
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(neptun_tests)
cmake_minimum_required(VERSION 3.21)
project(neptun)

set(CMAKE_CXX_FLAGS "-std=c++2a")
set(CMAKE_CXX_COMPILER clang++)

include_directories(.)

add_executable(neptun
        main.cc
        ring_buffer.h
        udp.h)


cmake_minimum_required(VERSION 3.18)
project(sockets2 CXX)

#set(CMAKE_CTEST_ARGUMENTS "--output-on-failure")
set(CMAKE_CTEST_ARGUMENTS "--verbose")

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

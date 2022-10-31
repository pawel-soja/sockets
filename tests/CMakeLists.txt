# GTest include code would
# go somewhere in here

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.0
)
FetchContent_MakeAvailable(googletest)
add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest_main)

# TcpSocket Test
add_executable(tcpsocket_test tcpsocket_test.cpp)
target_link_libraries(tcpsocket_test PRIVATE GTest::GTest sockets)
add_test(tcpsocket_test tcpsocket_test)

# TcpSocket TcpServer Test (Ping pong)
add_executable(tcpsocketserver_test tcpsocketserver_test.cpp)
target_link_libraries(tcpsocketserver_test PRIVATE GTest::GTest sockets)
add_test(tcpsocketserver_test tcpsocketserver_test)

# AncillaryTcpSocket Test
if(NOT WIN32)
  add_executable(ancillarysocketserver_test ancillarysocketserver_test.cpp)
  target_link_libraries(ancillarysocketserver_test PRIVATE GTest::GTest sockets)
  add_test(ancillarysocketserver_test ancillarysocketserver_test)
endif()

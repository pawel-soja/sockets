cmake_minimum_required(VERSION 3.18)
project(sockets CXX)

find_package(Threads REQUIRED)
add_library(${PROJECT_NAME} STATIC "")

# Headers
list(APPEND ${PROJECT_NAME}_HEADERS
    tcpsocket.h
    tcpserver.h
)

list(APPEND ${PROJECT_NAME}_PRIVATE_HEADERS
    tcpsocket_p.h
    socketaddress.h
)

# Sources
list(APPEND ${PROJECT_NAME}_SOURCES
    tcpsocket.cpp
    tcpserver.cpp
    socketaddress.cpp
)

if(WIN32)
    list(APPEND ${PROJECT_NAME}_SOURCES
        tcpsocket_win.cpp
    )
    target_link_libraries(${PROJECT_NAME} wsock32 ws2_32)
else()
    list(APPEND ${PROJECT_NAME}_SOURCES
        tcpsocket_unix.cpp
        ancillarytcpsocket.cpp
    )
    list(APPEND ${PROJECT_NAME}_HEADERS ancillarytcpsocket.h)
endif()

# Setup Target
target_sources(${PROJECT_NAME}
    PUBLIC
    tcpsocket.h
    PRIVATE
    ${${PROJECT_NAME}_SOURCES}
    tcpsocket_p.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC ./
)

target_link_libraries(${PROJECT_NAME} Threads::Threads)

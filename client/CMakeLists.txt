cmake_minimum_required(VERSION 3.10)
project(GomokuClient)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")
endif()

include_directories(${PROJECT_SOURCE_DIR}/..)

set(SOURCE_FILES
    main.cpp
    Client.cpp
    ../TcpSocket.cpp
)

set(HEADER_FILES
    Client.h
    ../TcpSocket.h
)

add_executable(gomoku_client ${SOURCE_FILES} ${HEADER_FILES})

if(WIN32)
    target_link_libraries(gomoku_client ws2_32)
else()
    target_link_libraries(gomoku_client pthread)
endif()

install(TARGETS gomoku_client DESTINATION bin)
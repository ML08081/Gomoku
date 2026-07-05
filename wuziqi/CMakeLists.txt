cmake_minimum_required(VERSION 3.10)
project(GomokuServer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread -O2")
endif()

set(SOURCE_FILES
    main.cpp
    Database.cpp
    TcpSocket.cpp
    GameLogic.cpp
    Server.cpp
    Logger.cpp
)

set(HEADER_FILES
    Database.h
    TcpSocket.h
    GameLogic.h
    Server.h
    Logger.h
)

add_executable(gomoku_server ${SOURCE_FILES} ${HEADER_FILES})

if(UNIX)
    target_include_directories(gomoku_server PRIVATE 
        /usr/include/mysql 
        /usr/include/mysql/jdbc
    )
    
    target_link_libraries(gomoku_server 
        mysqlcppconn 
        mysqlclient 
        crypto 
        pthread
    )
elseif(WIN32)
    find_path(MYSQL_INCLUDE_DIR mysql_connection.h 
        PATHS "C:/Program Files/MySQL/MySQL Connector C++ 8.0/include"
              "C:/Program Files (x86)/MySQL/MySQL Connector C++ 8.0/include"
    )
    
    find_library(MYSQLCPPCONN_LIB mysqlcppconn
        PATHS "C:/Program Files/MySQL/MySQL Connector C++ 8.0/lib64/vs14"
              "C:/Program Files (x86)/MySQL/MySQL Connector C++ 8.0/lib32/vs14"
    )
    
    if(MYSQL_INCLUDE_DIR AND MYSQLCPPCONN_LIB)
        target_include_directories(gomoku_server PRIVATE ${MYSQL_INCLUDE_DIR})
        target_link_libraries(gomoku_server ${MYSQLCPPCONN_LIB})
    else()
        message(WARNING "MySQL Connector/C++ not found. Please set MYSQL_INCLUDE_DIR and MYSQLCPPCONN_LIB manually.")
    endif()
    
    target_link_libraries(gomoku_server ws2_32)
endif()

install(TARGETS gomoku_server DESTINATION bin)
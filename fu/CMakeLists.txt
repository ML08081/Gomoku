cmake_minimum_required(VERSION 3.10)
project(GomokuClientQt)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Network Widgets)

set(SOURCE_FILES
    main.cpp
    mainwindow.cpp
    networkmanager.cpp
    gameboard.cpp
)

set(HEADER_FILES
    mainwindow.h
    networkmanager.h
    gameboard.h
)

add_executable(GomokuClient ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(GomokuClient
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::Widgets
)

if(WIN32)
    set_target_properties(GomokuClient PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

install(TARGETS GomokuClient DESTINATION bin)
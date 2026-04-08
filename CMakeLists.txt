cmake_minimum_required(VERSION 3.21)
project(CustomDesktopQt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
#target_link_libraries(CustomDesktopQt Qt6::Widgets Qt6::WinExtras)

add_executable(CustomDesktopQt
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/DesktopWidget.h
    src/DesktopWidget.cpp
    resources.qrc
)

target_link_libraries(CustomDesktopQt Qt6::Widgets)
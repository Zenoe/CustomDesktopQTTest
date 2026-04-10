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
    src/boxfilemanager.h
    src/boxfilemanager.cpp
    resources.qrc

# # wireguard
#     src/wireguard/WireGuardDll.h
#     src/wireguard/WireGuardDll.cpp
#     src/wireguard/TunnelManager.h
#     src/wireguard/TunnelManager.cpp
#     src/wireguard/ConfigManager.h
#     src/wireguard/ConfigManager.cpp
#     src/wireguard/ReconnectManager.h
#     src/wireguard/ReconnectManager.cpp
#     src/wireguard/StatsPoller.h
#     src/wireguard/StatsPoller.cpp
#     src/ui/TunnelPanel.h
#     src/ui/TunnelPanel.cpp
#     src/ui/ConfigEditorDialog.h
#     src/ui/ConfigEditorDialog.cpp
#     src/ui/StatusMonitorDialog.h
#     src/ui/StatusMonitorDialog.cpp
)

target_link_libraries(CustomDesktopQt Qt6::Widgets)

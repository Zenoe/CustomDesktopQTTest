cmake_minimum_required(VERSION 3.21)
project(XYBox)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    # Debug 配置强制生成完整 PDB 和禁用优化
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /MDd")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG")
    # 如果 DLL 也在项目里，也给它加上
    # set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/DEBUG")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# adding this cause spdlog.h cannot be found
#target_include_directories(XYBox PRIVATE BEFORE src/include)
find_package(spdlog CONFIG REQUIRED)
#target_link_libraries(XYBox Qt6::Widgets Qt6::WinExtras)
add_executable(XYBox
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/DesktopWidget.h
    src/DesktopWidget.cpp
    src/boxfilemanager.h
    src/boxfilemanager.cpp
    resources.qrc

# wireguard
    src/WireGuardTunnel.h
    src/WireGuardTunnel.cpp
    src/WireGuardConfig.h
    src/WireGuardConfig.cpp
    src/WireGuardManager.h
    src/WireGuardManager.cpp
    src/WireGuardConfigDialog.h
    src/WireGuardConfigDialog.cpp
    src/WireGuardConfigManager.h
    src/WireGuardConfigManager.cpp
    src/WireGuardStatusPanel.h
    src/WireGuardStatusPanel.cpp
    src/WireGuardServiceHost.h
    src/WireGuardServiceHost.cpp

    src/util/logger.h
    src/util/ConfigManager.h
    src/util/ConfigManager.cpp
)

# target_link_libraries(XYBox Qt6::Widgets)
target_link_libraries(XYBox PRIVATE
    Qt6::Widgets
    Qt6::Network
    advapi32      # SCM / service control
    #ws2_32        # Winsock (UAPI socket)
    spdlog::spdlog
)

target_include_directories(XYBox PRIVATE
    BEFORE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/include"          # your own headers
)
# Need admin for SCM service install — embed a manifest
if(WIN32)
    set(QT_NO_CREATE_MANIFEST ON)
    target_sources(XYBox PRIVATE src/app.manifest)
    #target_link_options(XYBox PRIVATE)
	target_link_options(XYBox PRIVATE "/MANIFESTUAC:NO")
endif()

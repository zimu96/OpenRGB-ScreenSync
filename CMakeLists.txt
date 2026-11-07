# OpenRGB-ScreenSync: spegne i LED di OpenRGB con lo schermo (dissolvenza).
# Backend: KDE Wayland, wlroots (sway/Hyprland/...), GNOME (D-Bus), X11 (i3, XFCE, ...).
# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.16)
project(ScreenSyncPlugin VERSION 1.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(X11LIBS QUIET x11 xext)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(GEN ${CMAKE_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})
foreach(P dpms wlr-output-power)
    add_custom_command(OUTPUT ${GEN}/${P}-client.h ${GEN}/${P}-protocol.c
        COMMAND ${WAYLAND_SCANNER} client-header ${CMAKE_SOURCE_DIR}/protocol/${P}.xml ${GEN}/${P}-client.h
        COMMAND ${WAYLAND_SCANNER} private-code  ${CMAKE_SOURCE_DIR}/protocol/${P}.xml ${GEN}/${P}-protocol.c
        DEPENDS ${CMAKE_SOURCE_DIR}/protocol/${P}.xml)
endforeach()

add_library(ScreenSyncPlugin MODULE
    src/ScreenSyncPlugin.cpp src/DpmsWatcher.cpp src/FadeEngine.cpp src/SettingsWidget.cpp
    ${GEN}/dpms-protocol.c ${GEN}/wlr-output-power-protocol.c)
target_include_directories(ScreenSyncPlugin PRIVATE src sdk ${GEN} ${WAYLAND_CLIENT_INCLUDE_DIRS})
target_link_libraries(ScreenSyncPlugin PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::DBus ${WAYLAND_CLIENT_LIBRARIES})
if(X11LIBS_FOUND)
    target_compile_definitions(ScreenSyncPlugin PRIVATE HAVE_X11)
    target_include_directories(ScreenSyncPlugin PRIVATE ${X11LIBS_INCLUDE_DIRS})
    target_link_libraries(ScreenSyncPlugin PRIVATE ${X11LIBS_LIBRARIES})
else()
    message(WARNING "X11/Xext non trovati: backend X11 (i3, XFCE...) disattivato")
endif()
set_target_properties(ScreenSyncPlugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

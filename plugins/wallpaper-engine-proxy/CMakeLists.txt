cmake_minimum_required(VERSION 3.16)
project(wallpaper-engine-proxy)

find_package(Qt6 REQUIRED COMPONENTS Quick DBus Widgets)
find_package(Dtk6 REQUIRED COMPONENTS Widget)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(ds-wallpaper-engine SHARED
    WallpaperEnginePlugin.cpp
    WallpaperEnginePlugin.h
    DBusClient.cpp
    DBusClient.h
    DDEEventBridge.cpp
    DDEEventBridge.h
    WallpaperListModel.cpp
    WallpaperListModel.h)

target_link_libraries(ds-wallpaper-engine PRIVATE
    Qt6::Quick
    Qt6::DBus
    Qt6::Widgets
    Dtk6::Widget
    dde-shell-frame)

# Install plugin
ds_install_package(PACKAGE org.deepin.ds.wallpaper-engine TARGET ds-wallpaper-engine)

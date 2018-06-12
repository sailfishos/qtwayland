QT += waylandclient-private

include(../../../../hardwareintegration/client/wayland-egl/wayland-egl.pri)

CONFIG += egl
QMAKE_USE += libdl

OTHER_FILES += \
    wayland-egl.json

SOURCES += main.cpp

TARGET = qt-plugin-wayland-egl

PLUGIN_TYPE = wayland-graphics-integration-client
PLUGIN_CLASS_NAME = QWaylandEglClientBufferPlugin
load(qt_plugin)

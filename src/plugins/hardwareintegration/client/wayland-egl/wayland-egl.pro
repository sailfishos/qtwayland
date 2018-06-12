QT += waylandclient-private

include(../../../../hardwareintegration/client/wayland-egl/wayland-egl.pri)

CONFIG += egl
QMAKE_USE += libdl

OTHER_FILES += \
    wayland-egl.json

SOURCES += main.cpp

PLUGIN_TYPE = wayland-graphics-integration-client
load(qt_plugin)

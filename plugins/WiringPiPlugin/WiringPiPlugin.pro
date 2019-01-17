QT += core
QT -= gui

TARGET = WiringPiPlugin
TEMPLATE = lib

DEFINES += WIRINGPIPLUGIN_LIBRARY

DESTDIR = $${OUT_PWD}/../../..

#Target version
VER_MAJ = 1
VER_MIN = 1
include(../../../common.pri)

SOURCES += \
    plugin.cpp

HEADERS += \
    plugin_global.h \
    plugin.h

OTHER_FILES = checkerinfo.json

LIBS += -lDai -lwiringPi

unix {
    CONFIG(debug, debug|release): QT += dbus

    target.path = /opt/dai/plugins
    INSTALLS += target
}

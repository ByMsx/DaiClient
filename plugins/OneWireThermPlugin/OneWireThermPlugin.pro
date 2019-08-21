QT += core
QT -= gui

TARGET = OneWireThermPlugin
TEMPLATE = lib

DEFINES += ONEWIRETHERMPLUGIN_LIBRARY

DESTDIR = $${OUT_PWD}/../../..

#Target version
VER_MAJ = 1
VER_MIN = 2
include(../../../common.pri)

SOURCES += \
    one_wire_therm_task.cpp \
    plugin.cpp

HEADERS += \
    one_wire_therm_task.h \
    plugin_global.h \
    plugin.h

OTHER_FILES = checkerinfo.json

LIBS += -lDai

unix {
    CONFIG(debug, debug|release): QT += dbus

    target.path = /opt/dai/plugins
    INSTALLS += target
}

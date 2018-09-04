#-------------------------------------------------
#
# Project created by QtCreator 2018-02-06T14:06:11
#
#-------------------------------------------------
QT += core serialport serialbus
QT -= gui

CONFIG(debug, debug|release) {
    QT += dbus
}

TARGET = ModbusPlugin
TEMPLATE = lib

DEFINES += MODBUSPLUGIN_LIBRARY

INCLUDEPATH += ../../lib/

#Target version
VER_MAJ = 1
VER_MIN = 1
include(../../common.pri)

SOURCES += modbusplugin.cpp

HEADERS += modbusplugin.h\
        modbusplugin_global.h

OTHER_FILES = checkerinfo.json

LIBS += -L$${DESTDIR}/../
LIBS += -lDai

unix {
    target.path = /opt/dai/plugins
    INSTALLS += target
}

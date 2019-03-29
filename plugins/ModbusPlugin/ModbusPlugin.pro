#-------------------------------------------------
#
# Project created by QtCreator 2018-02-06T14:06:11
#
#-------------------------------------------------
QT += core serialport serialbus
QT -= gui

TARGET = ModbusPlugin
TEMPLATE = lib

DEFINES += MODBUSPLUGIN_LIBRARY

DESTDIR = $${OUT_PWD}/../../..

#Target version
VER_MAJ = 1
VER_MIN = 1
include(../../../common.pri)

SOURCES += modbusplugin.cpp \
    modbus_file_writer.cpp

HEADERS += modbusplugin.h\
        modbusplugin_global.h \
    modbus_file_writer.h

OTHER_FILES = checkerinfo.json

LIBS += -lDai

unix {
    CONFIG(debug, debug|release): QT += dbus

    target.path = /opt/dai/plugins
    INSTALLS += target
}

QT += core
QT -= gui

TARGET = RandomPlugin
TEMPLATE = lib

DEFINES += RANDOMPLUGIN_LIBRARY

DESTDIR = $${OUT_PWD}/../../..

#Target version
VER_MAJ = 1
VER_MIN = 1
include(../../../common.pri)

SOURCES += randomplugin.cpp

HEADERS += randomplugin.h\
        randomplugin_global.h

OTHER_FILES = checkerinfo.json

LIBS += -lDai

unix {
    CONFIG += unversioned_libname unversioned_soname

    target.path = /opt/dai/plugins
    INSTALLS += target
}

QT += core
QT -= gui

TARGET = RandomPlugin
TEMPLATE = lib

DEFINES += RANDOMPLUGIN_LIBRARY

INCLUDEPATH += ../../../lib/

#Target version
VER_MAJ = 1
VER_MIN = 1
include(../../../common.pri)

SOURCES += randomplugin.cpp

HEADERS += randomplugin.h\
        randomplugin_global.h

OTHER_FILES = checkerinfo.json

LIBS += -L$${DESTDIR}/../../
LIBS += -lDai

unix {
    target.path = /opt/dai/plugins
    INSTALLS += target
}

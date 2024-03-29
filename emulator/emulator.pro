QT += core network gui sql serialport dbus serialbus websockets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DaiEmulator
TEMPLATE = app

DESTDIR = $${OUT_PWD}/../..

#Target version
VER_MAJ = 1
VER_MIN = 1
include(../../common.pri)

INCLUDEPATH += $${PWD}/..
LIBS += -lDai -lDaiPlus -lHelpzBase -lHelpzService -lHelpzDB -lHelpzNetwork -lbotan-2

SOURCES += main.cpp \
    ../Database/db_manager.cpp \
    mainwindow.cpp \
    mainbox.cpp

HEADERS  += \ 
    ../Database/db_manager.h \
    mainwindow.h \
    mainbox.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    main.qrc



QT += core network gui sql serialport dbus serialbus

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DaiEmulator
TEMPLATE = app

INCLUDEPATH += $${PWD}/../client/  ../client/Database/ \
                $${PWD}/../lib/

#Target version
VER_MAJ = 1
VER_MIN = 1
include(../common.pri)

LIBS += -L$${DESTDIR}
LIBS += -lDai -lHelpzService -lHelpzDB -lHelpzNetwork

SOURCES += main.cpp \
    ../client/Database/db_manager.cpp \
    mainwindow.cpp \
    mainbox.cpp

HEADERS  += \ 
    ../client/Database/db_manager.h \
    mainwindow.h \
    mainbox.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    main.qrc



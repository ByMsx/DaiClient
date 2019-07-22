QT += core network gui sql serialport dbus serialbus websockets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DaiEmulator
TEMPLATE = app

DESTDIR = $${OUT_PWD}/../..

#Target version
VER_MAJ = 1
VER_MIN = 2
include(../../common.pri)

INCLUDEPATH += $${PWD}/..
LIBS += -lDai -lDaiPlus -lHelpzBase -lHelpzService -lHelpzDBMeta -lHelpzDB -lHelpzNetwork -lbotan-2 -lboost_system

SOURCES += main.cpp \
    ../Database/db_manager.cpp \
    mainwindow.cpp \
    device_item_view.cpp \
    units_table_model.cpp \
    light_indicator.cpp

HEADERS  += \ 
    ../Database/db_manager.h \
    mainwindow.h \
    device_item_view.h \
    units_table_model.h \
    light_indicator.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    main.qrc



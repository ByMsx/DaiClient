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
    device_item_table_item.cpp \
    device_table_item.cpp \
    device_tree_view_delegate.cpp \
    devices_table_item.cpp \
    devices_table_model.cpp \
    mainwindow.cpp \
    register_table_item.cpp \
    registers_vector_item.cpp \
    light_indicator.cpp

HEADERS  += \
    device_item_table_item.h \
    device_table_item.h \
    device_tree_view_delegate.h \
    devices_table_item.h \
    devices_table_model.h \
    mainwindow.h \
    register_table_item.h \
    registers_vector_item.h \
    light_indicator.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    main.qrc



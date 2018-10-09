QT += core network dbus sql
QT -= gui

QT += script
#QT += qml

TARGET = DaiClient
CONFIG += console
CONFIG -= app_bundle

CONFIG (debug, debug|release) {
    CONFIG -= console
    CONFIG += app_bundle
}

TEMPLATE = app

INCLUDEPATH += ../lib/

SOURCES += main.cpp \
    DBus/d_iface.cpp \
    DBus/message.cpp \
    worker.cpp \
    Network/n_client.cpp \
    Database/db_manager.cpp \
    Scripts/tools/pidcontroller.cpp \
    Scripts/tools/automationhelper.cpp \
    Scripts/tools/resthelper.cpp \
    Scripts/tools/severaltimeshelper.cpp \
    Scripts/tools/pidhelper.cpp \
    Scripts/tools/daytimehelper.cpp \
    Scripts/tools/inforegisterhelper.cpp \
    Scripts/paramgroupclass.cpp \
    Scripts/paramgroupprototype.cpp \
    checker.cpp \
    Scripts/scriptedproject.cpp

HEADERS  += \
    DBus/d_iface.h \
    DBus/message.h \
    worker.h \
    Network/n_client.h \
    Database/db_manager.h \
    Scripts/tools/pidcontroller.h \
    Scripts/tools/automationhelper.h \
    Scripts/tools/resthelper.h \
    Scripts/tools/severaltimeshelper.h \
    Scripts/tools/pidhelper.h \
    Scripts/tools/daytimehelper.h \
    Scripts/tools/inforegisterhelper.h \
    Scripts/paramgroupprototype.h \
    Scripts/paramgroupclass.h \
    checker.h \
    Scripts/scriptedproject.h

#Target version
VER_MAJ = 1
VER_MIN = 2
include(../common.pri)

unix {
    target.path = /opt/dai
    INSTALLS += target
}

linux-rasp-pi2-g++|linux-opi-zero-g++|linux-rasp-pi3-g++ {
    LIBS += -L$${OUT_PWD}/../helpz/
    INCLUDEPATH += $$[QT_INSTALL_HEADERS]/botan-2
}

LIBS += -L$${DESTDIR}
LIBS += -lDai -lDaiPlus -lHelpzService -lHelpzNetwork -lHelpzDB -lHelpzDTLS -lbotan-2 -lboost_system -ldl

#DBUS_ADAPTORS += DBus/dai.xml
#message($$[QT_INSTALL_PREFIX])

DBUS_FILES += DBus/dai.xml
OTHER_FILES += $$DBUS_FILES ForServer/DAIClient

DBUS_INCLUDES = \
    plus/DBus/param.h
#    DBus/message.h

DBUS_TYPE = adaptor
include(DBus/mydbus.pri)

DISTFILES += \
    ForServer/Dai

RESOURCES += \
    main.qrc

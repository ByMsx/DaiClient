QT += core network sql websockets
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

SOURCES += main.cpp \
    worker.cpp \
    checker.cpp \
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
    Scripts/scriptedproject.cpp

HEADERS  += \
    worker.h \
    checker.h \
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
    INCLUDEPATH += $$[QT_INSTALL_HEADERS]/botan-2
}

LIBS += -lDai -lDaiPlus -lHelpzBase -lHelpzService -lHelpzNetwork -lHelpzDB -lHelpzDTLS -lbotan-2 -lboost_system -ldl

RESOURCES += \
    main.qrc

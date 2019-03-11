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
    Scripts/scriptedproject.cpp \
    Network/client_protocol.cpp \
    Network/client_protocol_2_0.cpp \
    Network/log_sender.cpp

HEADERS  += \
    worker.h \
    checker.h \
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
    Scripts/scriptedproject.h \
    Network/client_protocol.h \
    Network/client_protocol_2_0.h \
    Network/log_sender.h

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

LIBS += -lDai -lDaiPlus -lHelpzBase -lHelpzService -lHelpzNetwork -lHelpzDB -lHelpzDTLS -lbotan-2 -lboost_system -lboost_thread -ldl

RESOURCES += \
    main.qrc

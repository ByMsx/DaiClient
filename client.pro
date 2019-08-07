QT += core network sql websockets

QT += script
#QT += qml

TARGET = DaiClient
CONFIG += console
CONFIG -= app_bundle

CONFIG (debug, debug|release) {
    CONFIG -= console
    CONFIG += app_bundle
    QT += scripttools widgets
}

TEMPLATE = app

SOURCES += main.cpp \
    worker.cpp \
    checker.cpp \
    Database/db_manager.cpp \
    Scripts/tools/pidcontroller.cpp \
    Scripts/tools/automationhelper.cpp \
    Scripts/tools/severaltimeshelper.cpp \
    Scripts/tools/pidhelper.cpp \
    Scripts/tools/daytimehelper.cpp \
    Scripts/tools/inforegisterhelper.cpp \
    Scripts/paramgroupclass.cpp \
    Scripts/paramgroupprototype.cpp \
    Scripts/scriptedproject.cpp \
    Network/client_protocol.cpp \
    Network/log_sender.cpp \
    structure_synchronizer.cpp \
    Database/db_log_helper.cpp \
    websocket_item.cpp \
    log_value_save_timer.cpp \
    id_timer.cpp \
    Network/client_protocol_latest.cpp

HEADERS  += \
    worker.h \
    checker.h \
    Database/db_manager.h \
    Scripts/tools/pidcontroller.h \
    Scripts/tools/automationhelper.h \
    Scripts/tools/severaltimeshelper.h \
    Scripts/tools/pidhelper.h \
    Scripts/tools/daytimehelper.h \
    Scripts/tools/inforegisterhelper.h \
    Scripts/paramgroupprototype.h \
    Scripts/paramgroupclass.h \
    Scripts/scriptedproject.h \
    Network/client_protocol.h \
    Network/log_sender.h \
    structure_synchronizer.h \
    Database/db_log_helper.h \
    websocket_item.h \
    log_value_save_timer.h \
    id_timer.h \
    Network/client_protocol_latest.h

#Target version
VER_MAJ = 1
VER_MIN = 3
include(../common.pri)

unix {
    target.path = /opt/dai
    INSTALLS += target
}

linux-rasp-pi2-g++|linux-opi-zero-g++|linux-rasp-pi3-g++ {
    INCLUDEPATH += $$[QT_INSTALL_HEADERS]/botan-2
}

LIBS += -lDai -lDaiPlus -lHelpzBase -lHelpzService -lHelpzNetwork -lHelpzDBMeta -lHelpzDB -lHelpzDTLS -lbotan-2 -lboost_system -lboost_thread -ldl

RESOURCES += \
    main.qrc

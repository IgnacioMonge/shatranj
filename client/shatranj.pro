QT += widgets network

CONFIG += c++17
CONFIG -= app_bundle

TEMPLATE = app
TARGET = shatranj-client
SHATRANJ_VERSION = $$cat(../VERSION)
DEFINES += NETCHESSZX_APP_VERSION=$$SHATRANJ_VERSION

macx {
    CONFIG += app_bundle
    isEmpty(NETCHESSZX_MAC_APPLICATIONS_DIR): NETCHESSZX_MAC_APPLICATIONS_DIR = /Applications
    NETCHESSZX_MAC_BUNDLE = $$OUT_PWD/$${TARGET}.app
    NETCHESSZX_MAC_APP_LINK = $$NETCHESSZX_MAC_APPLICATIONS_DIR/Shatranj.app
    QMAKE_POST_LINK += mkdir -p $$shell_quote($$NETCHESSZX_MAC_APPLICATIONS_DIR) && ln -sfn $$shell_quote($$NETCHESSZX_MAC_BUNDLE) $$shell_quote($$NETCHESSZX_MAC_APP_LINK)
}

SOURCES += ../src/pc/client/main.cpp
INCLUDEPATH += ../src
INCLUDEPATH += ../third_party/mcu-max/src
SOURCES += ../src/common/chess/position.c
SOURCES += ../src/common/chess/legal.c
SOURCES += ../src/common/mqtt/mqtt.c
SOURCES += ../src/common/protocol/game_protocol.c
SOURCES += ../src/common/protocol/game_protocol_extra.c
SOURCES += ../src/common/protocol/mqtt_session_protocol.c
SOURCES += ../src/common/protocol/mqtt_session_protocol_format.c
SOURCES += ../src/common/protocol/direct_session_protocol.c
SOURCES += ../third_party/mcu-max/src/mcu-max.c

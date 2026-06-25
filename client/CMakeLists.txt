cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../VERSION" SHATRANJ_VERSION LIMIT_COUNT 1)
project(Shatranj VERSION ${SHATRANJ_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_standard_project_setup()
    qt_add_executable(shatranj-client
        ../src/pc/client/main.cpp
        ../src/common/chess/position.c
        ../src/common/chess/legal.c
        ../src/common/mqtt/mqtt.c
        ../src/common/protocol/game_protocol.c
        ../src/common/protocol/game_protocol_extra.c
        ../src/common/protocol/mqtt_session_protocol.c
        ../src/common/protocol/mqtt_session_protocol_format.c
        ../src/common/protocol/direct_session_protocol.c
        ../third_party/mcu-max/src/mcu-max.c
    )
else()
    add_executable(shatranj-client
        ../src/pc/client/main.cpp
        ../src/common/chess/position.c
        ../src/common/chess/legal.c
        ../src/common/mqtt/mqtt.c
        ../src/common/protocol/game_protocol.c
        ../src/common/protocol/game_protocol_extra.c
        ../src/common/protocol/mqtt_session_protocol.c
        ../src/common/protocol/mqtt_session_protocol_format.c
        ../src/common/protocol/direct_session_protocol.c
        ../third_party/mcu-max/src/mcu-max.c
    )
endif()

target_include_directories(shatranj-client PRIVATE
    ../src
    ../third_party/mcu-max/src
)

target_compile_definitions(shatranj-client PRIVATE
    NETCHESSZX_APP_VERSION=${PROJECT_VERSION}
)

target_link_libraries(shatranj-client PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

set_target_properties(shatranj-client PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(APPLE)
    set(NETCHESSZX_MAC_APPLICATIONS_DIR "/Applications" CACHE PATH
        "Directory where the macOS Applications entry is created")
    add_custom_command(TARGET shatranj-client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${NETCHESSZX_MAC_APPLICATIONS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                "$<TARGET_BUNDLE_DIR:shatranj-client>"
                "${NETCHESSZX_MAC_APPLICATIONS_DIR}/Shatranj.app"
        COMMENT "Updating macOS Applications entry: ${NETCHESSZX_MAC_APPLICATIONS_DIR}/Shatranj.app"
        VERBATIM
    )
endif()

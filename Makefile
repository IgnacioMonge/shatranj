CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -Isrc
BUILD_DIR := build
RELEASE_DIR := release
# Relative path from inside $(BUILD_DIR) back to the repo root; recipes
# cd into $(BUILD_DIR), which may be nested (e.g. build/nex).
empty :=
space := $(empty) $(empty)
BUILD_DIR_UP := $(subst $(space),,$(patsubst %,../,$(subst /, ,$(BUILD_DIR))))
ZCC ?= zcc
Z80ASM ?= z80asm
ZCC_ID = $(strip $(shell $(ZCC) 2>&1 | head -n 1))
Z80ASM_ID = $(strip $(shell $(Z80ASM) 2>&1 | head -n 1))
APPMAKE ?= z88dk-appmake
# Spectranext TAP loading screen: the installer's picture, as SpecTalkZX does.
SPXN_LOADING_SCREEN := assets/spectrum/installer.scr
POWERSHELL ?= pwsh
PYTHON ?= python3
CMAKE ?= cmake
CTEST ?= ctest
MACDEPLOYQT ?= macdeployqt
CODESIGN ?= codesign
DITTO ?= ditto
QTPATHS ?= qtpaths6
ZX_EXTRA_CFLAGS ?=
# SDCC allocation budget for Spectrum C: higher packs tighter code but
# compiles slower. `make release-check` builds with 200000.
MAX_ALLOCS_PER_NODE ?= 50000
CLIENT_BUILD ?= auto
CLIENT_CMAKE_BUILD_DIR ?= $(BUILD_DIR)/qt-client
CLIENT_CMAKE_CONFIG ?= Release
CLIENT_CMAKE_ARGS ?=
CLIENT_MSVC_QT_DIR ?= C:\Qt\6.11.0\msvc2022_64
CLIENT_MSVC_GENERATOR ?= Visual Studio 17 2022
CLIENT_MSVC_ARCH ?= x64
CLIENT_MSVC_CONFIG ?= Release
CLIENT_MSVC_CLEAN_TIMEOUT ?= 30
CLIENT_MAC_APPLICATIONS_DIR ?= /Applications
HOST_UNAME := $(shell uname -s 2>/dev/null || echo unknown)
CLIENT_HOST_IS_WINDOWS := $(if $(filter Windows_NT,$(OS)),1,$(if $(COMSPEC),1,$(if $(ComSpec),1,$(if $(filter MINGW% MSYS% CYGWIN%,$(HOST_UNAME)),1,))))
HOST_GC_CFLAGS := -ffunction-sections -fdata-sections
HOST_GC_LDFLAGS := -Wl,--gc-sections
HOST_INCLUDED_C_CFLAGS :=
# MinGW resolves PE/COFF references before --gc-sections; internalize product
# .c files included by host tests so only the exercised functions survive.
ifneq ($(CLIENT_HOST_IS_WINDOWS),)
HOST_INCLUDED_C_CFLAGS := -O1 -fwhole-program
endif
ifeq ($(HOST_UNAME),Darwin)
HOST_GC_LDFLAGS := -Wl,-dead_strip
endif

PORT ?= 5000
MQTT_HOST ?= broker.hivemq.com
MQTT_PORT ?= 1883
MQTT_CODE ?= NC0000
TIME_HOST ?= time.google.com
ZX_NAME := SHATRANJ
APP_VERSION := $(strip $(shell cat VERSION))
ZX_ORG := 28672
LOWMEM_LAYOUT ?= classic
ASSET_ASM := assets/spectrum/ui_runtime_assets.asm
ASM_DATA_TOOL := tools/asm_data.py
NEXT_UI_ASSET_ASM := assets/next/ui_runtime_assets.asm
ABOUT_BOARD := assets/spectrum/about_board.bin
ABOUT_BOARD_SRC := assets/spectrum/about_classic.scr
GAME_PROTOCOL_MACH_SRC := src/common/protocol/game_protocol_mach.c
SPECTRUM_INTERACTION_SRC := src/spectrum/ui/board_interaction.c src/spectrum/ui/cell_cursor.c src/spectrum/ui/chess_navigation.c
SPECTRUM_SRC := src/spectrum/app/app.c \
                src/spectrum/app/net_runtime.c \
                src/spectrum/app/time.c \
                src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                src/common/chess/move_coords.c \
                src/common/protocol/game_protocol.c \
                src/common/protocol/game_protocol_extra.c \
                src/common/protocol/direct_session_protocol.c \
                src/common/protocol/mqtt_session_protocol.c \
                src/spectrum/session/direct.c \
                src/spectrum/session/event.c \
                src/spectrum/session/mqtt.c \
                src/spectrum/session/outgoing.c \
                src/spectrum/session/ping.c \
                src/spectrum/session/poll.c \
                src/spectrum/platform/platform.c \
                src/spectrum/transport/esp_at.c \
                src/spectrum/transport/keepalive_protocol.c \
                src/spectrum/transport/net.c \
                src/spectrum/transport/mqtt_publish.c \
                src/spectrum/session/mqtt_wire.c \
                src/spectrum/transport/mqtt_min.c \
                src/spectrum/board/board.c \
                src/spectrum/saveload/saveload.c \
                src/spectrum/fileui/fileui.c \
                src/spectrum/restore/restore.c \
                src/spectrum/ui/gui.c \
                $(SPECTRUM_INTERACTION_SRC) src/spectrum/ui/setup.c \
                src/spectrum/overlay/overlay.c
PIECE_ASM := assets/spectrum/chess_pieces_16x16.asm
PIECE_PNGS := $(wildcard assets/spectrum/piece_sets/*/*.png)
SPECTRUM_HEADERS := $(shell find src/common src/spectrum \( -name '*.h' -o -name '*.inc' \) -print)
SESSION_APP_DEPS := $(wildcard src/spectrum/session/*.h src/spectrum/session/*.inc src/spectrum/app/*.h src/spectrum/app/*.inc) $(SPECTRUM_INTERACTION_SRC) src/spectrum/ui/setup.c src/spectrum/ui/interaction.h src/spectrum/ui/cell_cursor.h src/spectrum/ui/space_navigation.h src/common/space/space.h src/spectrum/ui/setup.h tests/spectrum/editor_fixture.h tests/spectrum/setup_fixture.h

RULES_TEST := $(BUILD_DIR)/netchesszx_rules_test.exe
RULES_TEST_SRC := src/common/chess/position.c src/common/chess/legal.c \
                  src/common/chess/rules_compact.c tests/rules/test_fen.c
ACTION_TEST := $(BUILD_DIR)/netchesszx_action_test.exe
ACTION_TEST_SRC := src/common/chess/legal.c src/common/chess/rules_compact.c \
                   src/common/chess/move_coords.c src/spectrum/board/board.c \
                   src/common/protocol/game_protocol.c \
                   src/common/protocol/game_protocol_format.c tests/rules/test_actions.c
MOVE_COORDS_TEST := $(BUILD_DIR)/netchesszx_move_coords_test.exe
MOVE_COORDS_TEST_SRC := src/common/chess/move_coords.c tests/common/test_move_coords.c
GAME_STATS_TEST := $(BUILD_DIR)/netchesszx_game_stats_test.exe
GAME_STATS_TEST_SRC := src/common/chess/game_stats.c tests/common/test_game_stats.c
SAVEGAME_WIRE_TEST := $(BUILD_DIR)/netchesszx_savegame_wire_test.exe
SAVEGAME_WIRE_TEST_SRC := src/common/savegame/savegame_wire.c \
                          src/spectrum/overlay/restore_ovl.c \
                          tests/common/test_savegame_wire.c
RECOVERY_CAPTURE_TEST := $(BUILD_DIR)/netchesszx_recovery_capture_test.exe
JOURNAL_TEST := $(BUILD_DIR)/netchesszx_journal_test.exe
JOURNAL_TEST_SRC := src/common/savegame/journal.c tests/common/test_journal.c
BOARD_TEST := $(BUILD_DIR)/netchesszx_spectrum_board_test.exe
BOARD_APPLY_IMPL := src/spectrum/board/board_apply_impl.h
BOARD_TEST_SRC := src/common/chess/move_coords.c \
                  src/spectrum/board/board.c src/spectrum/board/san.c \
                  src/common/chess/rules_compact.c \
                  src/common/chess/legal.c tests/spectrum/test_board.c
RULES_COMPACT_PERFT_TEST := $(BUILD_DIR)/netchesszx_rules_compact_perft_test.exe
RULES_COMPACT_PERFT_TEST_SRC := src/common/chess/move_coords.c \
                                src/spectrum/board/board.c \
                                src/common/chess/rules_compact.c \
                                tests/spectrum/test_rules_compact_perft.c
MQTT_TEST := $(BUILD_DIR)/netchesszx_mqtt_test.exe
MQTT_TEST_SRC := src/common/mqtt/mqtt.c src/spectrum/transport/mqtt_min.c \
                 tests/net/test_mqtt.c
MQTT_STREAM_TEST := $(BUILD_DIR)/netchesszx_mqtt_stream_test.exe
MQTT_STREAM_TEST_SRC := tests/spectrum/test_mqtt_stream.c
GAME_PROTOCOL_TEST := $(BUILD_DIR)/netchesszx_game_protocol_test.exe
GAME_PROTOCOL_TEST_SRC := src/common/protocol/game_protocol.c \
                           src/common/protocol/game_protocol_extra.c \
                           src/common/protocol/mqtt_session_protocol.c \
                           $(GAME_PROTOCOL_MACH_SRC) \
                           src/common/protocol/game_protocol_format.c \
                           tests/net/test_game_protocol.c
ESP_AT_TEST := $(BUILD_DIR)/netchesszx_esp_at_test.exe
ESP_AT_NEXT_TEST := $(BUILD_DIR)/netchesszx_esp_at_next_test.exe
ESP_AT_TEST_SRC := src/spectrum/transport/esp_at.c src/common/protocol/game_protocol.c tests/net/test_esp_at.c
UART_PLATFORM_NEXT_TEST := $(BUILD_DIR)/netchesszx_uart_platform_next_test.exe
MQTT_STREAM_NEXT_TEST := $(BUILD_DIR)/netchesszx_mqtt_stream_next_test.exe
UART_PLATFORM_TEST := $(BUILD_DIR)/netchesszx_uart_platform_test.exe
UART_PLATFORM_TEST_SRC := src/spectrum/platform/platform.c tests/spectrum/test_uart_platform.c
DIRECT_IPD_TEST := $(BUILD_DIR)/netchesszx_direct_ipd_test.exe
DIRECT_IPD_TEST_SRC := tests/spectrum/test_direct_ipd.c \
                       tests/spectrum/text_asm_host.c
MQTT_SESSION_PROTOCOL_TEST := $(BUILD_DIR)/netchesszx_mqtt_session_protocol_test.exe
MQTT_SESSION_PROTOCOL_TEST_SRC := src/common/protocol/mqtt_session_protocol.c \
                                  src/common/protocol/mqtt_session_protocol_format.c \
                                  tests/net/test_mqtt_session_protocol.c
DIRECT_SESSION_PROTOCOL_TEST := $(BUILD_DIR)/netchesszx_direct_session_protocol_test.exe
DIRECT_SESSION_PROTOCOL_TEST_SRC := src/common/protocol/direct_session_protocol.c \
                                    src/common/protocol/game_protocol.c \
                                    tests/net/test_direct_session_protocol.c
SESSION_CORE_TEST := $(BUILD_DIR)/netchesszx_session_core_test.exe
SESSION_CORE_TEST_SRC := src/common/session/session.c \
                         src/common/session/mqtt_session.c \
                         src/common/protocol/game_protocol.c \
                         src/common/protocol/game_protocol_extra.c \
                         src/common/protocol/game_protocol_format.c \
                         src/common/protocol/mqtt_session_protocol.c \
                         src/common/protocol/mqtt_session_protocol_format.c \
                         tests/session/test_session_core.c
SESSION_DIRECT_CORE_TEST := $(BUILD_DIR)/netchesszx_session_direct_core_test.exe
SESSION_DIRECT_CORE_TEST_SRC := src/common/session/session.c \
                                src/common/session/direct_session.c \
                                src/common/session/mqtt_session.c \
                                src/common/protocol/mqtt_session_protocol.c \
                                src/common/protocol/mqtt_session_protocol_format.c \
                                src/common/protocol/direct_session_protocol.c \
                                src/common/protocol/game_protocol.c \
                                src/common/protocol/game_protocol_extra.c \
                                src/common/protocol/game_protocol_format.c \
                                tests/session/test_direct_session_core.c
SESSION_MQTT_PARITY_TEST := $(BUILD_DIR)/netchesszx_mqtt_session_parity_test.exe
SESSION_MQTT_PARITY_60_TEST := $(BUILD_DIR)/netchesszx_mqtt_session_parity_60_test.exe
SESSION_MQTT_PARITY_NEXT_TEST := $(BUILD_DIR)/netchesszx_mqtt_session_parity_next_test.exe
SESSION_MQTT_PARITY_NEXT_60_TEST := $(BUILD_DIR)/netchesszx_mqtt_session_parity_next_60_test.exe
SESSION_MQTT_PARITY_TEST_SRC := src/common/session/session.c \
                                   src/common/session/mqtt_session.c \
                                   tests/session/test_mqtt_session_parity.c \
                                  tests/session/mqtt_session_transcripts.c \
                                  tests/spectrum/text_asm_host.c \
                                  src/common/chess/move_coords.c \
                                  src/common/chess/legal.c \
                                  src/common/protocol/direct_session_protocol.c \
                                  src/common/protocol/game_protocol.c \
                                  src/common/protocol/game_protocol_extra.c \
                                  $(GAME_PROTOCOL_MACH_SRC) \
                                  src/common/protocol/game_protocol_format.c \
                                  src/common/protocol/mqtt_session_protocol.c \
                                  src/common/protocol/mqtt_session_protocol_format.c \
                                  src/spectrum/app/time.c \
                                  src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                                  src/spectrum/session/direct.c \
                                  src/spectrum/session/event.c \
                                  src/spectrum/overlay/control_ovl.c \
                                  src/spectrum/session/mqtt.c \
                                  src/spectrum/session/outgoing.c \
                                  src/spectrum/session/ping.c \
                                  src/spectrum/session/poll.c \
                                  src/spectrum/transport/keepalive_protocol.c \
                                  src/spectrum/transport/mqtt_publish.c \
                                  src/spectrum/session/mqtt_wire.c \
                                  src/spectrum/overlay/restore_ovl.c \
                                  src/spectrum/board/board.c \
                                  src/spectrum/board/san.c \
                                  src/common/chess/rules_compact.c
SESSION_DIRECT_PARITY_TEST := $(BUILD_DIR)/netchesszx_direct_session_parity_test.exe
SESSION_DIRECT_PARITY_NEXT_TEST := $(BUILD_DIR)/netchesszx_direct_session_parity_next_test.exe
SESSION_DIRECT_PARITY_NEXT_60_TEST := $(BUILD_DIR)/netchesszx_direct_session_parity_next_60_test.exe
SESSION_DIRECT_PARITY_SPECTRANEXT_TEST := $(BUILD_DIR)/netchesszx_direct_session_parity_spectranext_test.exe
SESSION_DIRECT_PARITY_SPECTRANEXT_60_TEST := $(BUILD_DIR)/netchesszx_direct_session_parity_spectranext_60_test.exe
SESSION_DIRECT_PARITY_TEST_SRC := tests/session/test_direct_session_parity.c \
                                  tests/session/direct_reference_runner.c \
                                  tests/spectrum/text_asm_host.c \
                                  src/common/session/session.c \
                                  src/common/session/direct_session.c \
                                  src/common/session/mqtt_session.c \
                                  src/common/chess/move_coords.c \
                                  src/common/chess/legal.c \
                                  src/common/protocol/direct_session_protocol.c \
                                  src/common/protocol/mqtt_session_protocol_format.c \
                                  src/common/protocol/game_protocol.c \
                                  src/common/protocol/game_protocol_extra.c \
                                  $(GAME_PROTOCOL_MACH_SRC) \
                                  src/common/protocol/game_protocol_format.c \
                                  src/common/protocol/mqtt_session_protocol.c \
                                  src/spectrum/app/time.c \
                                  src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                                  src/spectrum/session/direct.c \
                                  src/spectrum/session/event.c \
                                  src/spectrum/overlay/control_ovl.c \
                                  src/spectrum/session/mqtt.c \
                                  src/spectrum/session/outgoing.c \
                                  src/spectrum/session/ping.c \
                                  src/spectrum/session/poll.c \
                                  src/spectrum/transport/keepalive_protocol.c \
                                  src/spectrum/overlay/restore_ovl.c \
                                  src/spectrum/board/board.c \
                                  src/spectrum/board/san.c \
                                  src/common/chess/rules_compact.c
KEEPALIVE_PROTOCOL_TEST := $(BUILD_DIR)/netchesszx_keepalive_protocol_test.exe
KEEPALIVE_PROTOCOL_TEST_SRC := src/spectrum/transport/keepalive_protocol.c \
                               src/common/protocol/game_protocol.c \
                               tests/net/test_keepalive_protocol.c
SESSION_CONFIG_TEST := $(BUILD_DIR)/netchesszx_session_config_test.exe
SESSION_CONFIG_TEST_SRC := src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                           src/common/protocol/game_protocol.c \
                           tests/spectrum/test_session_config.c
APP_CONFIG_FORMAT_TEST := $(BUILD_DIR)/netchesszx_app_config_format_test.exe
APP_CONFIG_FORMAT_TEST_SRC := tests/spectrum/test_app_config_format.c
CONFIG_OVERLAY_TEST := $(BUILD_DIR)/netchesszx_config_overlay_test.exe
CONFIG_OVERLAY_TEST_SRC := tests/spectrum/test_config_overlay.c \
                           src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                           src/common/protocol/game_protocol.c
SPECTRANEXT_CONFIG_TEST := $(BUILD_DIR)/netchesszx_spectranext_config_test.exe
SPECTRANEXT_CONFIG_TEST_SRC := src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                               src/common/protocol/game_protocol.c \
                               tests/spectrum/test_config_xfs.c
SPECTRANEXT_STORAGE_TEST := $(BUILD_DIR)/netchesszx_spectranext_storage_test.exe
SPECTRANEXT_STORAGE_TEST_SRC := tests/spectrum/test_saveload_xfs.c
SAVELOAD_ESXDOS_TEST := $(BUILD_DIR)/netchesszx_saveload_esxdos_test.exe
SAVELOAD_ESXDOS_TEST_SRC := tests/spectrum/test_saveload_esxdos.c
SESSION_PING_TEST := $(BUILD_DIR)/netchesszx_session_ping_test.exe
SESSION_PING_TEST_SRC := src/spectrum/session/ping.c tests/spectrum/test_session_ping.c
SESSION_PING_NEXT_TEST := $(BUILD_DIR)/netchesszx_session_ping_next_test.exe
SESSION_PING_SPECTRANEXT_TEST := $(BUILD_DIR)/netchesszx_session_ping_spectranext_test.exe
SESSION_POLL_TEST := $(BUILD_DIR)/netchesszx_session_poll_test.exe
MQTT_CONNECT_ACTIONS_TEST := $(BUILD_DIR)/netchesszx_mqtt_connect_actions_test.exe
SESSION_POLL_60_TEST := $(BUILD_DIR)/netchesszx_session_poll_60_test.exe
SESSION_POLL_SPECTRANEXT_TEST := $(BUILD_DIR)/netchesszx_session_poll_spectranext_test.exe
SESSION_POLL_SPECTRANEXT_60_TEST := $(BUILD_DIR)/netchesszx_session_poll_spectranext_60_test.exe
SESSION_POLL_TEST_SRC := src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                          src/common/protocol/direct_session_protocol.c \
                          src/common/protocol/game_protocol.c \
                          src/common/protocol/game_protocol_extra.c \
                          $(GAME_PROTOCOL_MACH_SRC) \
                          src/common/protocol/mqtt_session_protocol.c \
                         src/spectrum/transport/keepalive_protocol.c \
                         src/spectrum/session/mqtt.c src/spectrum/session/event.c \
                         src/spectrum/overlay/control_ovl.c \
                         src/spectrum/session/ping.c src/spectrum/session/poll.c \
                         tests/spectrum/test_session_poll.c
SESSION_DIRECT_TEST := $(BUILD_DIR)/netchesszx_session_direct_test.exe
SESSION_DIRECT_TEST_SRC := src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                            src/common/protocol/direct_session_protocol.c \
                            src/common/protocol/game_protocol.c \
                            src/spectrum/session/direct.c \
                            tests/spectrum/test_session_direct.c
SESSION_EVENT_TEST := $(BUILD_DIR)/netchesszx_session_event_test.exe
SESSION_EVENT_TEST_SRC := src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                           src/common/protocol/direct_session_protocol.c \
                           src/common/protocol/game_protocol.c \
                           src/common/protocol/game_protocol_extra.c \
                           $(GAME_PROTOCOL_MACH_SRC) \
                           src/common/protocol/mqtt_session_protocol.c \
                          src/spectrum/transport/keepalive_protocol.c \
                          src/spectrum/session/mqtt.c src/spectrum/session/event.c \
                          src/spectrum/overlay/control_ovl.c \
                          tests/spectrum/test_session_event.c
SESSION_MQTT_TEST := $(BUILD_DIR)/netchesszx_session_mqtt_test.exe
SESSION_MQTT_TEST_SRC := src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                         src/common/protocol/game_protocol.c \
                         src/common/protocol/mqtt_session_protocol.c \
                         src/spectrum/session/mqtt.c \
                         tests/spectrum/test_session_mqtt.c
SESSION_SPECTRUM_PAIR_TEST := $(BUILD_DIR)/netchesszx_session_spectrum_pair_test.exe
SESSION_SPECTRUM_PAIR_TEST_SRC := src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                                  src/common/protocol/direct_session_protocol.c \
                                  src/common/protocol/game_protocol.c \
                                  src/common/protocol/mqtt_session_protocol.c \
                                  src/common/protocol/mqtt_session_protocol_format.c \
                                  src/spectrum/session/direct.c \
                                  src/spectrum/session/mqtt.c \
                                  tests/spectrum/test_session_spectrum_pair.c
SESSION_OUTGOING_TEST := $(BUILD_DIR)/netchesszx_session_outgoing_test.exe
SESSION_OUTGOING_TEST_SRC := src/common/protocol/game_protocol.c \
                             src/spectrum/config/session.c src/spectrum/session/identity.c src/spectrum/session/state.c \
                             src/spectrum/session/outgoing.c \
                             tests/spectrum/text_asm_host.c \
                             tests/spectrum/test_session_outgoing.c
STATUS_OVERLAY_TEST := $(BUILD_DIR)/netchesszx_status_overlay_test.exe
STATUS_OVERLAY_TEST_SRC := tests/spectrum/test_status_overlay.c
GUI_LOG_TEST := $(BUILD_DIR)/netchesszx_gui_log_test.exe
GUI_LOG_TEST_SRC := tests/spectrum/test_gui_log.c
TIME_TEST := $(BUILD_DIR)/netchesszx_time_test.exe
TIME_60_TEST := $(BUILD_DIR)/netchesszx_time_60_test.exe
TIME_TEST_SRC := src/spectrum/app/time.c tests/spectrum/test_time.c
TIME_RUNTIME_TEST := $(BUILD_DIR)/netchesszx_time_runtime_test.exe
TIME_RUNTIME_60_TEST := $(BUILD_DIR)/netchesszx_time_runtime_60_test.exe
SESSION_TRANSITIONS_TEST := $(BUILD_DIR)/netchesszx_session_transitions_test.exe
SESSION_TRANSITIONS_TEST_SRC := tests/spectrum/test_session_transitions.c tests/spectrum/text_asm_host.c src/spectrum/session/state.c src/common/protocol/game_protocol.c src/common/protocol/game_protocol_extra.c
GUI_TIMER_TEST := $(BUILD_DIR)/netchesszx_gui_timer_test.exe
GUI_TIMER_60_TEST := $(BUILD_DIR)/netchesszx_gui_timer_60_test.exe
GUI_TIMER_TEST_SRC := tests/spectrum/test_gui_timer.c
FILEUI_POLICY_TEST := $(BUILD_DIR)/netchesszx_fileui_policy_test.exe
FILEUI_POLICY_TEST_SRC := tests/spectrum/test_fileui_policy.c
SPECTRANEXT_LINK_TEST := $(BUILD_DIR)/netchesszx_spectranext_link_test.exe
SPECTRANEXT_LINK_TEST_SRC := tests/spectrum/test_spectranext_link.c
ZX_TAP := $(RELEASE_DIR)/$(ZX_NAME).tap
ZX_OVL := $(RELEASE_DIR)/$(ZX_NAME).OVL
ZX_DAT := $(RELEASE_DIR)/$(ZX_NAME).DAT
BUILD_DAT := $(BUILD_DIR)/$(ZX_NAME).DAT
OVL_DEFS := $(BUILD_DIR)/overlay_defs.asm
OVL_ATLAS_TABLE := $(BUILD_DIR)/overlay_atlas_table.asm
ATLAS_SEED_FILE := tools/overlay_atlas_seeds.json
ATLAS_SEED_PROFILE ?= classic
OVERLAY_BLOCK_SIZE ?= 2048
OVERLAY_SIZE_LIMIT ?= $(OVERLAY_BLOCK_SIZE)
ATLAS_EXTRA ?= JOURNAL RECOVERY
ATLAS_BINDING_PATHS := Makefile VERSION src/common src/spectrum asm \
                       tools/gen_overlay_atlas.py tools/netchesszx_bool_copt \
                       tools/netchesszx_overlay_copt
ATLAS_BINDING_DEPS = Makefile VERSION $(SPECTRUM_SRC) $(SPECTRUM_HEADERS) \
                      $(OVERLAY_SRC) $(SHRINK_ASM) $(EDIT_FIELD_ASM) \
                      $(SAN_ASM) $(SCREEN_ASM) $(EDITOR_STATE_ASM) $(RESIDENT_EXTRA_ASM) \
                      $(RESIDENT_INCLUDE_DEPS) $(SCREEN_INCLUDE_DEPS) \
                      $(OVERLAY_LOADER_ASM) \
                      tools/gen_overlay_atlas.py tools/netchesszx_bool_copt \
                      tools/netchesszx_overlay_copt
BUILD_CONFIG_STAMP := $(BUILD_DIR)/spectrum_config.json
ABI_MANIFEST := $(BUILD_DIR)/abi_manifest.json
ABI_BASELINE := docs/abi_manifest.baseline.json
NEXT_ABI_MANIFEST = $(NEXT_NEX_BUILD_DIR)/abi_manifest.json
NEXT_ABI_BASELINE := docs/abi_manifest.next.baseline.json
ABI_HEADERS := src/spectrum/overlay/overlay_api.h \
               src/spectrum/overlay/overlay_context.h \
               src/spectrum/overlay/overlay.h \
               src/spectrum/ui/info_panel.h \
               src/spectrum/render_status.h \
               src/spectrum/lowram_map.h \
               src/common/session/session.h
SIZE_REPORT := $(BUILD_DIR)/size_report.json
SIZE_BASELINE := docs/size_report.baseline.json
LITERAL_REPORT := $(BUILD_DIR)/literal_report.json
ZX_BUILD_LOG := $(BUILD_DIR)/$(ZX_NAME).build.log

ZX_TARGET_CFLAGS ?=
UART_BACKEND ?= divmmc
NET_BACKEND ?= esp_at
FS_BACKEND ?= esxdos
SPXN_DIR ?=
ifeq ($(filter esp_at spectranext,$(NET_BACKEND)),)
$(error unsupported NET_BACKEND=$(NET_BACKEND))
endif
ifeq ($(filter esxdos xfs,$(FS_BACKEND)),)
$(error unsupported FS_BACKEND=$(FS_BACKEND))
endif
ifeq ($(filter divmmc next none,$(UART_BACKEND)),)
$(error unsupported UART_BACKEND=$(UART_BACKEND))
endif
ifeq ($(FS_BACKEND),xfs)
ifneq ($(NET_BACKEND),spectranext)
$(error FS_BACKEND=xfs requires NET_BACKEND=spectranext)
endif
endif
NEXT_VISUAL_ASSETS := assets/next/ui_theme.asm assets/next/theme_constants.asm \
                      assets/next/coord_palette.asm assets/next/hint_inks.asm \
                      assets/next/piece_set_menu.asm $(NEXT_UI_ASSET_ASM)
ifeq ($(NET_BACKEND),spectranext)
DAT_PLATFORM_FLAG := --spectranext
else ifeq ($(UART_BACKEND),next)
DAT_PLATFORM_FLAG := --next
ASSET_ASM := $(NEXT_UI_ASSET_ASM)
DAT_PLATFORM_DEPS := assets/next/ui_theme.asm
ATLAS_BINDING_PATHS += $(NEXT_VISUAL_ASSETS)
ATLAS_BINDING_DEPS += $(NEXT_VISUAL_ASSETS)
RESIDENT_INCLUDE_DEPS += $(NEXT_VISUAL_ASSETS)
else
DAT_PLATFORM_FLAG :=
endif
UI_FAMILY := $(if $(filter next,$(UART_BACKEND)),next,zx)
UI_LAYOUT_HEADER := src/spectrum/ui/layout_$(UI_FAMILY).h
UI_LAYOUT_ASM := $(BUILD_DIR)/ui_layout.inc
UI_LAYOUT_INPUTS := $(UI_LAYOUT_HEADER) src/spectrum/ui/log_contract.h tools/gen_ui_layout.py
UI_PRESENTATION_HEADERS := $(wildcard src/spectrum/ui/layout*.h src/spectrum/ui/presentation*.h) src/spectrum/ui/log_contract.h
SESSION_APP_DEPS += $(UI_PRESENTATION_HEADERS)
ifneq ($(UART_BACKEND),next)
SPECTRUM_HEADERS := $(filter-out src/spectrum/ui/layout_next.h src/spectrum/ui/presentation_next.h,$(SPECTRUM_HEADERS))
ATLAS_BINDING_EXCLUDES := --binding-exclude asm/next --binding-exclude src/spectrum/ui/layout_next.h --binding-exclude src/spectrum/ui/presentation_next.h
endif
ifeq ($(UART_BACKEND),none)
UART_ASM :=
else
UART_ASM := asm/uart/$(UART_BACKEND)_uart.asm
endif
SCREEN_ASM ?= $(if $(filter next,$(UART_BACKEND)),asm/next/screen.asm,asm/spectrum/screen.asm)
SCREEN_INCLUDE_DEPS := $(wildcard asm/ula/*.inc) asm/app/screen_dispatch.inc \
                       $(wildcard $(dir $(SCREEN_ASM))board_*.inc) \
                       $(wildcard $(dir $(SCREEN_ASM))piece_*.inc)
OVERLAY_LOADER_ASM ?= asm/esxdos/overlay_loader.asm
EDITOR_STATE_ASM := asm/spectrum/editor_state.asm
FRAME_CLOCK_ASM := asm/spectrum/frame_clock.asm
RESIDENT_EXTRA_ASM ?=
CLASSIC_BOOT_ASM :=
CLASSIC_BOOT_REFS_ASM :=
CLASSIC_BOOT :=
CLASSIC_BOOT_FLAG :=
ifeq ($(NET_BACKEND):$(FS_BACKEND):$(UART_BACKEND),esp_at:esxdos:divmmc)
CLASSIC_BOOT := 1
CLASSIC_BOOT_ASM := asm/spectrum/classic_boot.asm
CLASSIC_BOOT_REFS_ASM := asm/spectrum/classic_boot_refs.asm
CLASSIC_BOOT_FLAG := -DNETCHESSZX_CLASSIC_BOOT
ifeq ($(filter $(CLASSIC_BOOT_REFS_ASM),$(RESIDENT_EXTRA_ASM)),)
override RESIDENT_EXTRA_ASM += $(CLASSIC_BOOT_REFS_ASM)
endif
endif
RESIDENT_INCLUDE_DEPS += asm/spectrum/setup_state.inc asm/spectrum/timer_roll.asm asm/spectrum/taboption.asm asm/spectrum/timer_roll_layout.inc
JOURNAL_ASM := asm/spectrum/journal.asm
# Packaging (JOURNAL overlay in the atlas), resident hook (journal.asm +
# app.c calls + the append entry inside SAVELOAD) and codec placement (STJ1
# inside SAVELOAD entries 0/1) are separate capabilities. Classic derives the
# hook from the atlas extra; Next enables both explicitly and packages no
# JOURNAL overlay. ATLAS_JOURNAL only means the overlay really exists.
JOURNAL_HOOK ?= $(if $(filter JOURNAL,$(ATLAS_EXTRA)),1,0)
STJ_SAVELOAD ?= 0
JOURNAL_FLAG := $(if $(filter JOURNAL,$(ATLAS_EXTRA)),-DATLAS_JOURNAL)
RECOVERY_FLAG := $(if $(filter RECOVERY,$(ATLAS_EXTRA)),-DNETCHESSZX_RECOVERY)
JOURNAL_HOOK_FLAG := $(if $(filter 1,$(JOURNAL_HOOK)),-DNETCHESSZX_JOURNAL_HOOK)
STJ_SAVELOAD_FLAG := $(if $(filter 1,$(STJ_SAVELOAD)),-DSTJ_SAVELOAD)
ifneq ($(filter 1,$(JOURNAL_HOOK)),)
ifeq ($(findstring $(JOURNAL_ASM),$(RESIDENT_EXTRA_ASM)),)
override RESIDENT_EXTRA_ASM += $(JOURNAL_ASM)
endif
else
override RESIDENT_EXTRA_ASM := $(filter-out $(JOURNAL_ASM),$(RESIDENT_EXTRA_ASM))
endif
RESIDENT_INCLUDE_DEPS ?=
UX_ASM_INCLUDES := asm/app/board_hints_contract.inc asm/app/board_hints_show.inc \
                   asm/app/board_hints_clear.inc asm/spectrum/board_hints_view.inc \
                   $(wildcard asm/platform/input_*.inc)
UX_RULES_INCLUDES := asm/app/board_hints_contract.inc asm/app/rules_hints_coordinator.inc \
                     asm/app/rules_hints_state.inc asm/spectrum/rules_hints_view.inc
RESIDENT_INCLUDE_DEPS += $(UX_ASM_INCLUDES)
SHRINK_ASM := asm/spectrum/shrink_kernels.asm
EDIT_FIELD_ASM := asm/spectrum/edit_field.asm
EDIT_BUF_OVL := asm/overlay/edit/edit_buf.asm
SAN_ASM := asm/spectrum/san.asm
ESX_COMMON_ASM := asm/esxdos/esx_fileio_spectalk.asm
ESX_FILEUI_ASM := asm/esxdos/esx_fileui.asm
ESX_SAVELOAD_ASM := asm/esxdos/esx_saveload.asm
ESX_FILEUI_OBJ := asm/esxdos/esx_fileui.o
ESX_SAVELOAD_OBJ := asm/esxdos/esx_saveload.o
SPXN_DIR_ABS :=
SPXN_RESIDENT_C :=
SPXN_RESIDENT_ASM :=
SPXN_RESIDENT_OBJS :=
SPXN_RESIDENT_LINK_OBJS :=
SPXN_CLOCK_C :=
SPXN_CLOCK_HEADERS :=
SPXN_TIME_OVERLAY_SRC :=
SPXN_ATOMIC_SRC :=
SPXN_ATOMIC_OBJ :=
SPXN_RESOLVE_C :=
SPXN_XFS_SOURCE :=
SPXN_XFS_OVERLAY_ASM :=

ifeq ($(NET_BACKEND),spectranext)
ifeq ($(strip $(SPXN_DIR)),)
$(error NET_BACKEND=spectranext requires SPXN_DIR=/path/to/SpectraNext/driver)
endif
ifneq ($(FS_BACKEND),xfs)
$(error NET_BACKEND=spectranext requires FS_BACKEND=xfs)
endif
ifneq ($(UART_BACKEND),none)
$(error NET_BACKEND=spectranext requires UART_BACKEND=none)
endif
SPXN_DIR_ABS := $(abspath $(SPXN_DIR))
SPECTRUM_SRC := $(filter-out src/spectrum/transport/esp_at.c,$(SPECTRUM_SRC))
SPXN_RESIDENT_C := $(SPXN_DIR_ABS)/spxn.c \
                   $(SPXN_DIR_ABS)/spxn_stream.c
SPXN_RESIDENT_ASM := $(SPXN_DIR_ABS)/spxn_rom.asm \
                     asm/esxdos/xfs_loader_spectranext.asm
SPXN_RESIDENT_OBJS := $(BUILD_DIR)/spxn.o \
                      $(BUILD_DIR)/spxn_stream.o \
                      $(BUILD_DIR)/spxn_rom.o \
                      $(BUILD_DIR)/xfs_loader_spectranext.o
SPXN_RESIDENT_LINK_OBJS := $(notdir $(SPXN_RESIDENT_OBJS))
SPXN_CLOCK_C := $(SPXN_DIR_ABS)/spxudp.c $(SPXN_DIR_ABS)/spxtime.c
SPXN_CLOCK_HEADERS := $(SPXN_DIR_ABS)/spxudp.h \
                      $(SPXN_DIR_ABS)/spxtime.h \
                      $(SPXN_DIR_ABS)/spxn_rom.h \
                      $(SPXN_DIR_ABS)/spxn.h
# The atomic replace unit is a standalone driver file: linking it with
# spxn_rom.asm pulls neither spxf.c nor its handle state, so it fits inside
# the CONFIG and SAVELOAD overlay slots and costs the resident nothing.
SPXN_ATOMIC_SRC := $(SPXN_DIR_ABS)/spxf_replace.asm
SPXN_ATOMIC_OBJ := $(BUILD_DIR)/spxf_replace_ovl.o
SPXN_RESOLVE_C := $(SPXN_DIR_ABS)/spxresolve.c
SPXN_XFS_SOURCE := $(SPXN_DIR_ABS)/adapters/xfs_compat.asm
SPXN_XFS_OVERLAY_ASM := $(BUILD_DIR)/xfs_compat.asm
SPXN_TIME_OVERLAY_SRC := asm/overlay/time/entry_time.asm \
                         src/spectrum/overlay/time_ovl.c \
                         $(SPXN_CLOCK_C) \
                         $(SPXN_CLOCK_HEADERS)
ATLAS_BINDING_PATHS += $(SPXN_RESIDENT_C) $(SPXN_RESIDENT_ASM) \
                       $(SPXN_RESOLVE_C) $(SPXN_XFS_SOURCE) \
                       $(SPXN_CLOCK_C) $(SPXN_CLOCK_HEADERS)
ATLAS_BINDING_DEPS += $(SPXN_RESIDENT_C) $(SPXN_RESIDENT_ASM) \
                      $(SPXN_RESOLVE_C) $(SPXN_XFS_SOURCE)
ESX_COMMON_ASM :=
ESX_FILEUI_ASM :=
ESX_SAVELOAD_ASM :=
ESX_FILEUI_OBJ :=
ESX_SAVELOAD_OBJ :=
ZX_TARGET_CFLAGS += -DNETCHESSZX_SPECTRANEXT -DNETCHESSZX_FS_XFS \
                    -DNETCHESSZX_TZ=0 \
                    -pragma-define:REGISTER_SP=0 -I$(SPXN_DIR_ABS)
endif

NEXT_OVERLAY_LOADER_ASM := asm/next/overlay_loader_next.asm
NEXT_GRAPHICS_BANK_ASM := asm/next/extension_bank_next.asm
NEXT_GRAPHICS_BANK_LAYOUT := asm/next/extension_bank_layout.asm
NEXT_EXTENSION_INCLUDES := asm/next/screen_extension_stubs_top.asm \
                           asm/next/screen_extension_stubs_tail.asm $(NEXT_VISUAL_ASSETS) \
                           $(wildcard asm/next/board_*.inc) asm/next/screen.asm \
                           src/spectrum/ui/layout_next.h
NEXT_RESIDENT_ASM := asm/next/graphics_bank_next.asm
NEXT_NEX_BUILD_DIR := $(BUILD_DIR)/nex
NEXT_NEX_STAGE_DIR := $(NEXT_NEX_BUILD_DIR)/stage
NEXT_NEX_RELEASE_DIR := release/Next
NEXT_SIZE_REPORT := $(NEXT_NEX_BUILD_DIR)/size_report.json
NEXT_GRAPHICS_BANK_DEFS := $(NEXT_NEX_BUILD_DIR)/next_graphics_defs.asm
NEXT_GRAPHICS_BANK_OBJ := $(NEXT_NEX_BUILD_DIR)/asm/next/extension_bank_next.o
NEXT_GRAPHICS_BANK_BIN := $(NEXT_NEX_BUILD_DIR)/SHATRANJ_EXTENSION.BIN
NEXT_GRAPHICS_BANK_ORG := 0x2000
NEXT_GRAPHICS_BANK_OFFSET := 0
NEXT_GRAPHICS_BANK_LIMIT := 5376
NEXT_EXTENSION_FREE_MIN := 512
NEXT_MIN_SP_GAP := 3072
NEXT_ZX_ORG := 28672
ZX0 ?= z88dk-zx0
NEXT_ZX0 ?= $(ZX0)
NEXT_BUNDLE_BANK_BASE := 8
NEXT_RAW_BANK_BASE := 16
NEXT_MAX_BUNDLE_BANKS := 4
NEXT_OVERLAY_OFFSET := 98304
NEXT_BUNDLE_DAT_OFFSET := 8192
NEXT_SPRITE_BIN := assets/next/lichess_piece_sprites.bin
NEXT_SPRITE_PALETTE_ASM := assets/next/lichess_sprite_palette.asm
NEXT_SPRITE_PAL_BIN := assets/next/lichess_sprite_palette.bin
NEXT_SPRITE_META := assets/next/lichess_piece_sprites.json
NEXT_SPRITE_SOURCE_ASSETS := $(wildcard assets/pc-client/piece_sets/*/*.svg) \
                             $(wildcard assets/pc-client/boards/*.png) \
                             $(wildcard assets/pc-client/boards/*.jpg) \
                             $(wildcard assets/pc-client/boards/*.jpeg)
NEXT_SPRITE_OFFSET := 16384
NEXT_SPRITE_PAL_OFFSET := 32768
NEXT_ABOUT_NXI_SRC := assets/next/about_screen.nxi
NEXT_ABOUT_NXI := $(NEXT_NEX_BUILD_DIR)/about_screen_baked.nxi
NEXT_ABOUT_PAL_OFFSET := 33792
NEXT_ABOUT_PIXELS_OFFSET := 49152
ZX_NEX := $(NEXT_NEX_RELEASE_DIR)/$(ZX_NAME).nex

ZX_CLIB := sdcc_iy
ZX_ASMFLAGS := -DNETCHESSZX_SDCC_IY $(JOURNAL_FLAG) $(RECOVERY_FLAG) $(JOURNAL_HOOK_FLAG) $(STJ_SAVELOAD_FLAG) $(CLASSIC_BOOT_FLAG)
ifeq ($(NET_BACKEND),spectranext)
# No overlay calls the adapter's esx_freplace (saves use spxf_replace_atomic),
# so every overlay copy of xfs_compat.asm leaves it out.
ZX_ASMFLAGS += -DNETCHESSZX_SPECTRANEXT -DNETCHESSZX_FS_XFS \
               -DSPXN_ROM_HELD -DSPXN_ROM_HELD_EXTERNAL -DSPXN_XFS_NO_REPLACE \
               -DSPXN_XFS_STATE_BASE=0x5B50 \
               -DSPXN_XFS_DIR_SCRATCH=0x662B \
               -DSPXN_XFS_SCRATCH_PRESERVE_SIZE=0
endif
# The .nex banking build (Next loader) also flags standalone-assembled ASM
# overlays so they select the MMU-specific variants.
ifeq ($(OVERLAY_LOADER_ASM),$(NEXT_OVERLAY_LOADER_ASM))
ZX_ASMFLAGS += -DNETCHESSZX_NEXT -DNETCHESSZX_NEXT_BANKING
else ifeq ($(NET_BACKEND)/$(UART_BACKEND),esp_at/divmmc)
# Classic starts the stack at the top of RAM and keeps the DIRECT receive
# slots above the IM2 vector (asm/esxdos/overlay_loader.asm).
ZX_ASMFLAGS += -DNETCHESSZX_HIGH_SCRATCH
ZX_TARGET_CFLAGS += -DNETCHESSZX_HIGH_SCRATCH -pragma-define:REGISTER_SP=0
endif
ZX_SDCC_CFLAGS := -compiler=sdcc -Cs--no-reg-params --opt-code-size --fomit-frame-pointer \
                  -Cs--max-allocs-per-node$(MAX_ALLOCS_PER_NODE) \
                  -DNETCHESSZX_SDCC_IY $(JOURNAL_FLAG) $(RECOVERY_FLAG) $(JOURNAL_HOOK_FLAG) $(STJ_SAVELOAD_FLAG) $(CLASSIC_BOOT_FLAG) \
                  $(addprefix -Ca,$(ZX_ASMFLAGS))
ZX_LDFLAGS := -Wl,--gc-sections
ZX_Z80ASM := $(Z80ASM) $(ZX_ASMFLAGS)

ZX_CFLAGS := -vn -startup=31 -clib=$(ZX_CLIB) -SO3 -m \
             -custom-copt-rules=$(BUILD_DIR_UP)tools/netchesszx_bool_copt \
             $(ZX_SDCC_CFLAGS) \
             -Ca-I$(patsubst %/,%,$(BUILD_DIR_UP)) \
             -I$(BUILD_DIR_UP)src \
             -pragma-define:CLIB_MALLOC_HEAP_SIZE=0 \
             -pragma-define:CLIB_STDIO_HEAP_SIZE=0 \
             -pragma-define:CRT_ENABLE_STDIO=0 \
             -pragma-define:CRT_ENABLE_EIDI=0 \
             -pragma-define:CRT_STACK_SIZE=512 \
              -DNETCHESSZX_FIXED_LOW_RAM \
             -zorg=$(ZX_ORG) \
             -DNETCHESSZX_PORT=$(PORT) \
             -DNETCHESSZX_MQTT_HOST_TOKEN=$(MQTT_HOST) \
             -DNETCHESSZX_MQTT_PORT=$(MQTT_PORT) \
             -DNETCHESSZX_MQTT_CODE_TOKEN=$(MQTT_CODE) \
              $(ZX_LDFLAGS) \
              $(ZX_EXTRA_CFLAGS) \
              $(ZX_TARGET_CFLAGS)
ZX_OVL_CFLAGS := -vn -clib=$(ZX_CLIB) -SO3 -m \
                  -custom-copt-rules=$(BUILD_DIR_UP)tools/netchesszx_overlay_copt \
                  $(ZX_SDCC_CFLAGS) \
                  -I$(BUILD_DIR_UP)src \
                -DNETCHESSZX_FIXED_LOW_RAM \
                  $(ZX_EXTRA_CFLAGS) \
                  $(ZX_TARGET_CFLAGS)
MQTT_CONNECT_OVL_CFLAGS := -DNETCHESSZX_MQTT_PORT=$(MQTT_PORT)
CONFIG_OVL_CFLAGS := -DNETCHESSZX_MQTT_CODE_TOKEN=$(MQTT_CODE)
SPXN_CLOCK_OVL_CFLAGS := --opt-code-size \
                         -DNETCHESSZX_TIME_HOST_TOKEN=$(TIME_HOST)
SPXN_RESIDENT_CFLAGS := $(filter-out -m,$(ZX_OVL_CFLAGS))

OVERLAY_SRC := asm/overlay/rules/entry_rules.asm \
               asm/overlay/rules/rules_stub.asm \
               $(UX_RULES_INCLUDES) \
               asm/overlay/board/entry_board.asm \
               asm/overlay/board/helpers.asm \
               src/spectrum/overlay/board_apply_ovl.c \
               $(BOARD_APPLY_IMPL) \
               asm/overlay/gui_log/entry_gui_log.asm \
               src/spectrum/overlay/gui_log_ovl.c \
               asm/overlay/input_edit/entry_input_edit.asm \
               asm/overlay/input_edit/setup_edit_line.asm \
               src/spectrum/overlay/input_edit_ovl.c \
               $(EDIT_BUF_OVL) \
               asm/overlay/mqtt_connect/entry_mqtt_connect.asm \
               src/spectrum/overlay/mqtt_connect_ovl.c \
               src/spectrum/session/mqtt_connect_actions_ovl.c \
               asm/overlay/mqtt_tx/entry_mqtt_tx.asm \
               src/spectrum/overlay/mqtt_tx_ovl.c \
               src/spectrum/session/mqtt_tx_actions_ovl.c \
               $(SPXN_TIME_OVERLAY_SRC) \
               $(SPXN_ATOMIC_SRC) \
               $(SPXN_RESOLVE_C) \
               $(SPXN_XFS_OVERLAY_ASM) \
               asm/overlay/direct/entry_direct.asm \
               src/spectrum/overlay/direct_ovl.c \
               asm/overlay/menu_config/entry_menu_config.asm \
               asm/overlay/menu_logic/entry_menu_logic.asm \
               src/spectrum/overlay/status_ovl.c \
               asm/overlay/fileui/entry_fileui.asm \
               src/spectrum/overlay/fileui_ovl.c \
               asm/overlay/setup/entry_setup.asm \
               asm/overlay/saveload/entry_saveload.asm \
               src/spectrum/overlay/saveload_ovl.c \
               asm/overlay/restore/entry_restore.asm \
               src/spectrum/overlay/restore_ovl.c \
               asm/overlay/about/entry_about.asm \
               asm/overlay/control/entry_control.asm \
               src/spectrum/overlay/control_ovl.c \
               asm/overlay/config/entry_config.asm \
               src/spectrum/overlay/config_ovl.c \
               asm/overlay/time_config/entry_time_config.asm \
               asm/overlay/recovery/entry_recovery.asm \
               asm/overlay/recovery/rcv_store.asm \
               asm/overlay/recovery/rcv_resume.asm \
               asm/overlay/recovery/rcv_layout.inc \
               asm/overlay/saveload/journal_append.asm \
               asm/overlay/journal/stj_codec.asm \
               asm/overlay/journal/stj_saveload.asm \
               asm/esxdos/esx_fsync.asm \
               src/spectrum/overlay/recovery_ovl.c \
               $(GAME_PROTOCOL_MACH_SRC) \
               $(ESX_COMMON_ASM) \
               $(ESX_FILEUI_ASM) \
               $(ESX_SAVELOAD_ASM) \
               src/spectrum/overlay/overlay_api.h

NEXT_NEX_CONFIG_STAMP := $(NEXT_NEX_BUILD_DIR)/nex_config.json
# Every overlay ASM source/include: build_overlays.py links some files that no
# entry INCLUDEs (e.g. saveload/game_stats.asm), and INCLUDEd pages such as
# about/game_stats_render.asm are invisible to an explicit list. A missing one
# left the NEX stale while Classic (which tracks INCLUDEs itself) rebuilt.
NEXT_OVERLAY_ASM_DEPS := $(wildcard asm/overlay/*/*.asm asm/overlay/*/*.inc)
NEXT_NEX_INPUTS := Makefile $(SPECTRUM_SRC) $(SPECTRUM_HEADERS) $(OVERLAY_SRC) $(NEXT_OVERLAY_ASM_DEPS) \
                   $(SCREEN_INCLUDE_DEPS) $(UI_LAYOUT_INPUTS) \
                   $(SHRINK_ASM) $(EDIT_FIELD_ASM) $(SAN_ASM) \
                   $(RESIDENT_INCLUDE_DEPS) \
                   asm/spectrum/screen.asm asm/spectrum/text.asm \
                   $(NEXT_OVERLAY_LOADER_ASM) $(NEXT_RESIDENT_ASM) \
                   asm/uart/next_uart.asm \
                   tools/netchesszx_bool_copt tools/netchesszx_overlay_copt \
                   tools/check_sdcc_iy_contract.py \
                   tools/check_tap_image.py tools/gen_overlay_defs.py \
                   tools/gen_overlay_atlas.py tools/gen_assets.py \
                   tools/build_overlays.py \
                   $(ATLAS_SEED_FILE) $(ASSET_ASM) $(PIECE_ASM) \
                   $(ABOUT_BOARD) VERSION

.NOTPARALLEL:

.PHONY: all check full-check module-guards layering-check layering-report overlay-cap-check overlay-cap-report overlay-entry-abi-check transport-contract-check mqtt-client-id-check pc-direct-policy-check pc-policy-guard spectrum-direct-policy-check session-boundaries-check test spectranext-config-test spectranext-storage-test spectranext-network-test spectranext-conformance-test spectranext-clock-test spectranext-port-build spectranext-direct spectranext-pages status-overlay-test gui-log-test time-test gui-timer-test fileui-policy-test session-core-test session-mqtt-parity-test session-direct-core-test session-direct-parity-test spectrum-rules-asm-test rules-deep rules-oracle session-spectrum-pair-test tap tap-direct-overlay tap-divmmc tap-spectranext spectranext-size-report next-size-report next nex nex-size-report overlay-size sdcc-iy-contract-check abi-manifest abi-next-manifest abi-baseline abi-next-baseline abi-check abi-next-check size-report size-baseline size-check literal-report client client-msvc client-cmake clean clean-spectrum clean-client FORCE

all: NEXT_STATIC_TESTS_DONE := 1
all: check test tap nex client

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

ifneq ($(SPXN_DIR_ABS),)
$(BUILD_DIR)/spxn.o: $(SPXN_DIR_ABS)/spxn.c $(SPXN_DIR_ABS)/spxn.h $(SPXN_DIR_ABS)/spxn_internal.h $(SPXN_DIR_ABS)/spxn_rom.h Makefile | $(BUILD_DIR)
	cd $(BUILD_DIR) && $(ZCC) +z80 $(SPXN_RESIDENT_CFLAGS) -c $(SPXN_DIR_ABS)/spxn.c -o spxn.o

$(BUILD_DIR)/spxn_stream.o: $(SPXN_DIR_ABS)/spxn_stream.c $(SPXN_DIR_ABS)/spxn_stream.h $(SPXN_DIR_ABS)/spxn.h Makefile | $(BUILD_DIR)
	cd $(BUILD_DIR) && $(ZCC) +z80 $(SPXN_RESIDENT_CFLAGS) -c $(SPXN_DIR_ABS)/spxn_stream.c -o spxn_stream.o

$(BUILD_DIR)/spxn_rom.o: $(SPXN_DIR_ABS)/spxn_rom.asm Makefile | $(BUILD_DIR)
	cp -f $(SPXN_DIR_ABS)/spxn_rom.asm $(BUILD_DIR)/spxn_rom.asm
	cd $(BUILD_DIR) && $(ZX_Z80ASM) spxn_rom.asm

$(BUILD_DIR)/xfs_loader_spectranext.o: asm/esxdos/xfs_loader_spectranext.asm Makefile | $(BUILD_DIR)
	cp -f asm/esxdos/xfs_loader_spectranext.asm $(BUILD_DIR)/xfs_loader_spectranext.asm
	cd $(BUILD_DIR) && $(ZX_Z80ASM) xfs_loader_spectranext.asm

$(BUILD_DIR)/xfs_compat.asm: $(SPXN_DIR_ABS)/adapters/xfs_compat.asm Makefile | $(BUILD_DIR)
	cp -f $(SPXN_DIR_ABS)/adapters/xfs_compat.asm $(BUILD_DIR)/xfs_compat.asm
endif

$(RULES_TEST): $(RULES_TEST_SRC) src/common/chess/position.h src/common/chess/legal.h src/common/chess/rules_compact.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(RULES_TEST_SRC) -o $@

$(RULES_TEST) $(BOARD_TEST) $(RULES_COMPACT_PERFT_TEST): src/common/chess/action.h
$(ACTION_TEST): $(ACTION_TEST_SRC) $(BOARD_APPLY_IMPL) src/common/chess/action.h src/common/chess/legal.h src/spectrum/board/board.h src/common/protocol/game_protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST $(ACTION_TEST_SRC) -o $@

.PHONY: action-test
action-test: $(ACTION_TEST)
	./$(ACTION_TEST)

test: action-test

$(MOVE_COORDS_TEST): $(MOVE_COORDS_TEST_SRC) src/common/chess/move_coords.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(MOVE_COORDS_TEST_SRC) -o $@

$(GAME_STATS_TEST): $(GAME_STATS_TEST_SRC) src/common/chess/game_stats.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GAME_STATS_TEST_SRC) -o $@

.PHONY: game-stats-test
game-stats-test: $(GAME_STATS_TEST)
	./$(GAME_STATS_TEST)

test: game-stats-test

$(SAVEGAME_WIRE_TEST): $(SAVEGAME_WIRE_TEST_SRC) src/common/savegame/savegame_wire.h src/common/savegame/savegame_format.h src/spectrum/restore/restore.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -D__z88dk_fastcall= \
		$(SAVEGAME_WIRE_TEST_SRC) -o $@

$(RECOVERY_CAPTURE_TEST): tests/spectrum/test_recovery_capture.c src/spectrum/overlay/recovery_ovl.c src/spectrum/overlay/restore_ovl.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
		tests/spectrum/test_recovery_capture.c -o $@

$(JOURNAL_TEST): $(JOURNAL_TEST_SRC) src/common/savegame/journal.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(JOURNAL_TEST_SRC) -o $@

$(BOARD_TEST): $(BOARD_TEST_SRC) $(BOARD_APPLY_IMPL) src/spectrum/board/board.h src/spectrum/board/san.h src/common/chess/rules_compact.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST $(BOARD_TEST_SRC) -o $@

$(RULES_COMPACT_PERFT_TEST): $(RULES_COMPACT_PERFT_TEST_SRC) $(BOARD_APPLY_IMPL) src/spectrum/board/board.h src/common/chess/rules_compact.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST $(RULES_COMPACT_PERFT_TEST_SRC) -o $@

$(MQTT_TEST): $(MQTT_TEST_SRC) src/common/mqtt/mqtt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(MQTT_TEST_SRC) -o $@

$(MQTT_STREAM_TEST): $(MQTT_STREAM_TEST_SRC) src/spectrum/transport/net.c src/spectrum/overlay/mqtt_connect_ovl.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-pointer-to-int-cast -DNETCHESSZX_HOST_TEST -D__z88dk_fastcall= \
		$(HOST_INCLUDED_C_CFLAGS) $(HOST_GC_CFLAGS) $(MQTT_STREAM_TEST_SRC) $(HOST_GC_LDFLAGS) -o $@


$(MQTT_STREAM_NEXT_TEST): $(MQTT_STREAM_TEST_SRC) src/spectrum/transport/net.c src/spectrum/overlay/mqtt_connect_ovl.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-pointer-to-int-cast -DNETCHESSZX_HOST_TEST -DNETCHESSZX_NEXT -D__z88dk_fastcall= \
		$(HOST_INCLUDED_C_CFLAGS) $(HOST_GC_CFLAGS) $(MQTT_STREAM_TEST_SRC) $(HOST_GC_LDFLAGS) -o $@

$(UART_PLATFORM_NEXT_TEST): $(UART_PLATFORM_TEST_SRC) src/spectrum/platform/uart.h src/spectrum/lowram_map.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_NEXT $(UART_PLATFORM_TEST_SRC) -o $@

$(GAME_PROTOCOL_TEST) $(ESP_AT_TEST) $(ESP_AT_NEXT_TEST): src/common/text.h

$(GAME_PROTOCOL_TEST): $(GAME_PROTOCOL_TEST_SRC) src/common/protocol/game_protocol.h src/common/protocol/platform_protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GAME_PROTOCOL_TEST_SRC) -o $@

$(ESP_AT_TEST): $(ESP_AT_TEST_SRC) src/spectrum/transport/esp_at.h src/spectrum/platform/net_runtime.h src/spectrum/platform/uart.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST $(ESP_AT_TEST_SRC) -o $@

$(ESP_AT_NEXT_TEST): $(ESP_AT_TEST_SRC) src/spectrum/transport/esp_at.h src/spectrum/platform/net_runtime.h src/spectrum/platform/uart.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-unused-function -DNETCHESSZX_HOST_TEST -DNETCHESSZX_NEXT $(ESP_AT_TEST_SRC) -o $@

$(UART_PLATFORM_TEST): $(UART_PLATFORM_TEST_SRC) src/spectrum/platform/uart.h src/spectrum/lowram_map.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST $(UART_PLATFORM_TEST_SRC) -o $@

$(DIRECT_IPD_TEST): $(DIRECT_IPD_TEST_SRC) src/spectrum/overlay/direct_ovl.c src/spectrum/overlay/overlay_api.h src/spectrum/transport/net.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -D__z88dk_fastcall= $(DIRECT_IPD_TEST_SRC) -o $@

$(MQTT_SESSION_PROTOCOL_TEST): $(MQTT_SESSION_PROTOCOL_TEST_SRC) src/common/protocol/mqtt_session_protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(MQTT_SESSION_PROTOCOL_TEST_SRC) -o $@

$(DIRECT_SESSION_PROTOCOL_TEST): $(DIRECT_SESSION_PROTOCOL_TEST_SRC) src/common/protocol/direct_session_protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DIRECT_SESSION_PROTOCOL_TEST_SRC) -o $@

$(SESSION_CORE_TEST): $(SESSION_CORE_TEST_SRC) src/common/session/session.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_CORE_TEST_SRC) -o $@

session-core-test: $(SESSION_CORE_TEST)
	./$(SESSION_CORE_TEST)

$(SESSION_MQTT_PARITY_60_TEST) $(SESSION_MQTT_PARITY_NEXT_60_TEST): CFLAGS += -DNETCHESSZX_SESSION_FRAME_HZ=60
$(SESSION_MQTT_PARITY_NEXT_TEST) $(SESSION_MQTT_PARITY_NEXT_60_TEST): CFLAGS += -DNETCHESSZX_NEXT -DNETCHESSZX_NEXT_BANKING

$(SESSION_MQTT_PARITY_TEST) $(SESSION_MQTT_PARITY_60_TEST) $(SESSION_MQTT_PARITY_NEXT_TEST) $(SESSION_MQTT_PARITY_NEXT_60_TEST): $(SESSION_MQTT_PARITY_TEST_SRC) $(BOARD_APPLY_IMPL) tests/session/mqtt_session_transcripts.h src/spectrum/app/app.c $(SESSION_APP_DEPS) src/spectrum/app/time.h src/spectrum/session/timing.h src/spectrum/overlay/mqtt_connect_ovl.c src/spectrum/overlay/mqtt_tx_ovl.c src/spectrum/session/mqtt_connect_actions_ovl.c src/spectrum/session/mqtt_tx_actions_ovl.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_HOST_SESSION_TEST \
		-D__z88dk_fastcall= -include tests/session/host_overlay_context.h $(HOST_GC_CFLAGS) \
		$(SESSION_MQTT_PARITY_TEST_SRC) $(SPECTRUM_INTERACTION_SRC) src/spectrum/ui/setup.c $(HOST_GC_LDFLAGS) -o $@

session-mqtt-parity-test: $(SESSION_MQTT_PARITY_TEST) $(SESSION_MQTT_PARITY_60_TEST) $(SESSION_MQTT_PARITY_NEXT_TEST) $(SESSION_MQTT_PARITY_NEXT_60_TEST)
	./$(SESSION_MQTT_PARITY_TEST)
	./$(SESSION_MQTT_PARITY_60_TEST)
	./$(SESSION_MQTT_PARITY_NEXT_TEST)
	./$(SESSION_MQTT_PARITY_NEXT_60_TEST)

$(SESSION_DIRECT_CORE_TEST): $(SESSION_DIRECT_CORE_TEST_SRC) src/common/session/session.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_DIRECT_CORE_TEST_SRC) -o $@

session-direct-core-test: $(SESSION_DIRECT_CORE_TEST)
	./$(SESSION_DIRECT_CORE_TEST)

$(SESSION_DIRECT_PARITY_TEST): $(SESSION_DIRECT_PARITY_TEST_SRC) $(BOARD_APPLY_IMPL) tests/session/direct_parity.h src/spectrum/app/app.c $(SESSION_APP_DEPS) src/spectrum/app/time.h src/spectrum/overlay/direct_ovl.c src/spectrum/session/timing.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_HOST_SESSION_TEST -DNETCHESSZX_RECOVERY \
		-D__z88dk_fastcall= $(HOST_GC_CFLAGS) \
		$(SESSION_DIRECT_PARITY_TEST_SRC) $(SPECTRUM_INTERACTION_SRC) src/spectrum/ui/setup.c $(HOST_GC_LDFLAGS) -o $@

$(SESSION_DIRECT_PARITY_NEXT_TEST): $(SESSION_DIRECT_PARITY_TEST_SRC) $(BOARD_APPLY_IMPL) tests/session/direct_parity.h src/spectrum/app/app.c $(SESSION_APP_DEPS) src/spectrum/app/time.h src/spectrum/overlay/direct_ovl.c src/spectrum/session/timing.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_HOST_SESSION_TEST \
		-DNETCHESSZX_NEXT -DNETCHESSZX_NEXT_BANKING -DNETCHESSZX_RECOVERY \
		-D__z88dk_fastcall= $(HOST_GC_CFLAGS) \
		$(SESSION_DIRECT_PARITY_TEST_SRC) $(SPECTRUM_INTERACTION_SRC) src/spectrum/ui/setup.c $(HOST_GC_LDFLAGS) -o $@

$(SESSION_DIRECT_PARITY_NEXT_60_TEST): $(SESSION_DIRECT_PARITY_TEST_SRC) $(BOARD_APPLY_IMPL) tests/session/direct_parity.h src/spectrum/app/app.c $(SESSION_APP_DEPS) src/spectrum/app/time.h src/spectrum/overlay/direct_ovl.c src/spectrum/session/timing.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_HOST_SESSION_TEST \
		-DNETCHESSZX_NEXT -DNETCHESSZX_NEXT_BANKING -DNETCHESSZX_RECOVERY \
		-DNETCHESSZX_SESSION_FRAME_HZ=60 \
		-D__z88dk_fastcall= $(HOST_GC_CFLAGS) \
		$(SESSION_DIRECT_PARITY_TEST_SRC) $(SPECTRUM_INTERACTION_SRC) src/spectrum/ui/setup.c $(HOST_GC_LDFLAGS) -o $@

$(SESSION_DIRECT_PARITY_SPECTRANEXT_TEST): $(SESSION_DIRECT_PARITY_TEST_SRC) $(BOARD_APPLY_IMPL) tests/session/direct_parity.h src/spectrum/app/app.c $(SESSION_APP_DEPS) src/spectrum/app/time.h src/spectrum/overlay/direct_ovl.c src/spectrum/session/timing.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_HOST_SESSION_TEST \
		-DNETCHESSZX_SPECTRANEXT -DNETCHESSZX_RECOVERY \
		-D__z88dk_fastcall= $(HOST_GC_CFLAGS) \
		$(SESSION_DIRECT_PARITY_TEST_SRC) $(SPECTRUM_INTERACTION_SRC) src/spectrum/ui/setup.c $(HOST_GC_LDFLAGS) -o $@

$(SESSION_DIRECT_PARITY_SPECTRANEXT_60_TEST): $(SESSION_DIRECT_PARITY_TEST_SRC) $(BOARD_APPLY_IMPL) tests/session/direct_parity.h src/spectrum/app/app.c $(SESSION_APP_DEPS) src/spectrum/app/time.h src/spectrum/overlay/direct_ovl.c src/spectrum/session/timing.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_HOST_SESSION_TEST \
		-DNETCHESSZX_SPECTRANEXT -DNETCHESSZX_RECOVERY -DNETCHESSZX_SESSION_FRAME_HZ=60 \
		-D__z88dk_fastcall= $(HOST_GC_CFLAGS) \
		$(SESSION_DIRECT_PARITY_TEST_SRC) $(SPECTRUM_INTERACTION_SRC) src/spectrum/ui/setup.c $(HOST_GC_LDFLAGS) -o $@

session-direct-parity-test: $(SESSION_DIRECT_PARITY_TEST)
	./$(SESSION_DIRECT_PARITY_TEST)

$(KEEPALIVE_PROTOCOL_TEST): $(KEEPALIVE_PROTOCOL_TEST_SRC) src/spectrum/transport/keepalive_protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(KEEPALIVE_PROTOCOL_TEST_SRC) -o $@

$(SESSION_CONFIG_TEST): $(SESSION_CONFIG_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h src/spectrum/transport/net.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_CONFIG_TEST_SRC) -o $@

$(APP_CONFIG_FORMAT_TEST): $(APP_CONFIG_FORMAT_TEST_SRC) src/spectrum/config/app_config_format.h src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(APP_CONFIG_FORMAT_TEST_SRC) -o $@

$(CONFIG_OVERLAY_TEST): $(CONFIG_OVERLAY_TEST_SRC) src/spectrum/overlay/config_ovl.c src/spectrum/config/app_config_format.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CONFIG_OVERLAY_TEST_SRC) -o $@

$(SPECTRANEXT_CONFIG_TEST): $(SPECTRANEXT_CONFIG_TEST_SRC) src/spectrum/overlay/config_ovl.c src/spectrum/config/app_config_format.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
		$(SPECTRANEXT_CONFIG_TEST_SRC) -o $@

spectranext-config-test: $(SPECTRANEXT_CONFIG_TEST)
	./$(SPECTRANEXT_CONFIG_TEST)

$(SPECTRANEXT_STORAGE_TEST): $(SPECTRANEXT_STORAGE_TEST_SRC) src/spectrum/overlay/saveload_ovl.c src/common/savegame/savegame_format.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
		$(SPECTRANEXT_STORAGE_TEST_SRC) -o $@

spectranext-storage-test: $(SPECTRANEXT_STORAGE_TEST) tests/tools/test_spectrum_asm_vectors.py
	./$(SPECTRANEXT_STORAGE_TEST)
	"$(PYTHON)" -c "import runpy; runpy.run_path('tests/tools/test_spectrum_asm_vectors.py')['run_spectranext_storage_source_guard']()"

$(SAVELOAD_ESXDOS_TEST): $(SAVELOAD_ESXDOS_TEST_SRC) src/spectrum/overlay/saveload_ovl.c src/common/savegame/savegame_format.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
		$(SAVELOAD_ESXDOS_TEST_SRC) -o $@

$(SESSION_PING_TEST): $(SESSION_PING_TEST_SRC) src/spectrum/session/ping.h src/spectrum/session/timing.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_PING_TEST_SRC) -o $@

$(SESSION_PING_NEXT_TEST): $(SESSION_PING_TEST_SRC) src/spectrum/session/ping.h src/spectrum/session/timing.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_NEXT $(SESSION_PING_TEST_SRC) -o $@

$(SESSION_PING_SPECTRANEXT_TEST): $(SESSION_PING_TEST_SRC) src/spectrum/session/ping.h src/spectrum/session/timing.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_SPECTRANEXT $(SESSION_PING_TEST_SRC) -o $@

$(MQTT_CONNECT_ACTIONS_TEST): tests/spectrum/test_mqtt_connect_actions.c src/spectrum/session/mqtt_connect_actions_ovl.c src/spectrum/session/mqtt_wire.c src/spectrum/session/identity.c tests/spectrum/text_asm_host.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_SESSION_TEST -D__z88dk_fastcall= $(HOST_INCLUDED_C_CFLAGS) $(HOST_GC_CFLAGS) tests/spectrum/test_mqtt_connect_actions.c $(HOST_GC_LDFLAGS) -o $@

test: $(MQTT_CONNECT_ACTIONS_TEST)

$(SESSION_POLL_TEST): $(SESSION_POLL_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h src/spectrum/session/poll.h src/spectrum/session/ping.h src/spectrum/session/timing.h src/spectrum/session/event.h src/spectrum/transport/link.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_POLL_TEST_SRC) -o $@

$(SESSION_POLL_60_TEST): $(SESSION_POLL_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h src/spectrum/session/poll.h src/spectrum/session/ping.h src/spectrum/session/timing.h src/spectrum/session/event.h src/spectrum/transport/link.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_SESSION_FRAME_HZ=60 $(SESSION_POLL_TEST_SRC) -o $@

$(SESSION_POLL_SPECTRANEXT_TEST): $(SESSION_POLL_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h src/spectrum/session/poll.h src/spectrum/session/ping.h src/spectrum/session/timing.h src/spectrum/session/event.h src/spectrum/transport/link.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_SPECTRANEXT $(SESSION_POLL_TEST_SRC) -o $@

$(SESSION_POLL_SPECTRANEXT_60_TEST): $(SESSION_POLL_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h src/spectrum/session/poll.h src/spectrum/session/ping.h src/spectrum/session/timing.h src/spectrum/session/event.h src/spectrum/transport/link.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_SPECTRANEXT \
		-DNETCHESSZX_SESSION_FRAME_HZ=60 $(SESSION_POLL_TEST_SRC) -o $@

$(SESSION_DIRECT_TEST): $(SESSION_DIRECT_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h src/spectrum/session/direct.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_DIRECT_TEST_SRC) -o $@

$(SESSION_EVENT_TEST): $(SESSION_EVENT_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h src/spectrum/session/event.h src/spectrum/session/mqtt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_EVENT_TEST_SRC) -o $@

$(SESSION_MQTT_TEST): $(SESSION_MQTT_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h src/spectrum/session/mqtt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_MQTT_TEST_SRC) -o $@

$(SESSION_SPECTRUM_PAIR_TEST): $(SESSION_SPECTRUM_PAIR_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h src/spectrum/session/direct.h src/spectrum/session/mqtt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_SPECTRUM_PAIR_TEST_SRC) -o $@

$(SESSION_OUTGOING_TEST): $(SESSION_OUTGOING_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h src/spectrum/session/outgoing.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_OUTGOING_TEST_SRC) -o $@

$(STATUS_OVERLAY_TEST): $(STATUS_OVERLAY_TEST_SRC) src/spectrum/overlay/status_ovl.c src/spectrum/overlay/overlay_context.h src/spectrum/render_status.h src/spectrum/session/event.h src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -D__z88dk_fastcall= \
		$(STATUS_OVERLAY_TEST_SRC) -o $@

status-overlay-test: $(STATUS_OVERLAY_TEST)
	./$(STATUS_OVERLAY_TEST)

$(GUI_LOG_TEST): $(GUI_LOG_TEST_SRC) src/spectrum/overlay/gui_log_ovl.c src/spectrum/overlay/overlay_context.h $(UI_PRESENTATION_HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -D__z88dk_fastcall= \
		-D__z88dk_callee= -Wno-int-to-pointer-cast \
		$(GUI_LOG_TEST_SRC) -o $@

gui-log-test: $(GUI_LOG_TEST)
	./$(GUI_LOG_TEST)

$(TIME_60_TEST) $(GUI_TIMER_60_TEST) $(TIME_RUNTIME_60_TEST): CFLAGS += -DNETCHESSZX_SESSION_FRAME_HZ=60

$(TIME_RUNTIME_TEST) $(TIME_RUNTIME_60_TEST): tests/spectrum/test_time_runtime.c src/spectrum/app/time.c src/spectrum/app/net_runtime.c src/spectrum/app/time.h src/spectrum/app/time_view.h src/spectrum/platform/net_runtime.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST tests/spectrum/test_time_runtime.c src/spectrum/app/time.c src/spectrum/app/net_runtime.c -o $@

$(TIME_TEST) $(TIME_60_TEST) $(GUI_TIMER_TEST) $(GUI_TIMER_60_TEST): src/spectrum/app/time_view.h src/spectrum/platform/net_runtime.h src/spectrum/ui/gui.h

$(TIME_TEST) $(TIME_60_TEST): $(TIME_TEST_SRC) src/spectrum/app/time.h src/spectrum/session/timing.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TIME_TEST_SRC) -o $@

$(SESSION_TRANSITIONS_TEST): $(SESSION_TRANSITIONS_TEST_SRC) $(SESSION_APP_DEPS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -O1 -DNETCHESSZX_HOST_TEST \
		-D__z88dk_fastcall= $(SESSION_TRANSITIONS_TEST_SRC) -o $@

.PHONY: session-transitions-test
session-transitions-test: $(SESSION_TRANSITIONS_TEST)
	./$(SESSION_TRANSITIONS_TEST)

time-test: $(TIME_TEST) $(TIME_60_TEST) $(TIME_RUNTIME_TEST) $(TIME_RUNTIME_60_TEST)
	./$(TIME_TEST)
	./$(TIME_60_TEST)
	./$(TIME_RUNTIME_TEST)
	./$(TIME_RUNTIME_60_TEST)

$(GUI_TIMER_TEST) $(GUI_TIMER_60_TEST): $(GUI_TIMER_TEST_SRC) src/spectrum/app/time.c src/spectrum/app/time.h src/spectrum/session/timing.h src/spectrum/ui/gui.c src/spectrum/ui/chess_move_view.inc src/spectrum/app/net_runtime.c src/spectrum/overlay/mqtt_connect_ovl.c $(UI_PRESENTATION_HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -D__z88dk_fastcall= \
		-D__z88dk_callee= -Wno-int-to-pointer-cast \
		$(HOST_INCLUDED_C_CFLAGS) $(HOST_GC_CFLAGS) $(GUI_TIMER_TEST_SRC) \
		$(HOST_GC_LDFLAGS) -o $@

SPACE_RECT_SRC := src/common/space/rect.c
SPACE_HEADERS := $(wildcard src/common/space/*.h)

$(BUILD_DIR)/netchesszx_space_rect_test.exe: tests/common/test_space_rect.c $(SPACE_RECT_SRC) $(SPACE_HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(SPACE_RECT_SRC) -o $@

SPACE_DEMO_SRC := examples/space_demo/demo.c src/spectrum/ui/cell_cursor.c
SPACE_DEMO_HEADERS := examples/space_demo/demo.h src/spectrum/ui/cell_cursor.h src/spectrum/ui/space_navigation.h $(SPACE_HEADERS)

$(BUILD_DIR)/netchesszx_space_demo_test.exe: tests/common/test_space_demo.c $(SPACE_DEMO_SRC) $(SPACE_RECT_SRC) $(SPACE_DEMO_HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iexamples/space_demo $< $(SPACE_DEMO_SRC) $(SPACE_RECT_SRC) -o $@

$(BUILD_DIR)/netchesszx_cell_cursor_test.exe: tests/spectrum/test_cell_cursor.c src/spectrum/ui/cell_cursor.c $(SPACE_DEMO_HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< src/spectrum/ui/cell_cursor.c -o $@

.PHONY: space-test
space-test: $(BUILD_DIR)/netchesszx_space_rect_test.exe $(BUILD_DIR)/netchesszx_space_demo_test.exe $(BUILD_DIR)/netchesszx_cell_cursor_test.exe
	./$(BUILD_DIR)/netchesszx_space_rect_test.exe
	./$(BUILD_DIR)/netchesszx_space_demo_test.exe
	./$(BUILD_DIR)/netchesszx_cell_cursor_test.exe

test: space-test

.PHONY: space-demo-tap
space-demo-tap: $(SPACE_DEMO_SRC) $(SPACE_RECT_SRC) $(SPACE_DEMO_HEADERS) examples/space_demo/spectrum.c | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/space-demo
	$(ZCC) +zx -vn -startup=31 -clib=sdcc_iy -compiler=sdcc -Cs--no-reg-params -SO3 \
		-DNETCHESSZX_SDCC_IY -Isrc -Iexamples/space_demo -m \
		examples/space_demo/spectrum.c $(SPACE_DEMO_SRC) $(SPACE_RECT_SRC) \
		-o $(BUILD_DIR)/space-demo/SPACEDEMO -create-app

.PHONY: space-demo-check
space-demo-check: space-demo-tap
	"$(PYTHON)" tests/tools/test_space_demo_z80.py

$(BUILD_DIR)/netchesszx_ux_interaction_test.exe: tests/spectrum/test_ux_interaction.c $(SPECTRUM_INTERACTION_SRC) src/spectrum/ui/interaction.h src/spectrum/ui/cell_cursor.h src/spectrum/ui/space_navigation.h src/spectrum/app/ux_ports.h $(SPACE_RECT_SRC) $(SPACE_HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST $< $(SPECTRUM_INTERACTION_SRC) $(SPACE_RECT_SRC) -o $@

$(BUILD_DIR)/netchesszx_ux_editor_test.exe: tests/spectrum/test_ux_editor.c src/spectrum/overlay/input_edit_ovl.c tests/spectrum/editor_fixture.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -D__z88dk_fastcall= -Wno-int-to-pointer-cast $(HOST_INCLUDED_C_CFLAGS) $(HOST_GC_CFLAGS) $< $(HOST_GC_LDFLAGS) -o $@

ux-interaction-test: $(BUILD_DIR)/netchesszx_ux_interaction_test.exe $(BUILD_DIR)/netchesszx_ux_editor_test.exe
	./$(BUILD_DIR)/netchesszx_ux_interaction_test.exe
	./$(BUILD_DIR)/netchesszx_ux_editor_test.exe

test: ux-interaction-test

$(BUILD_DIR)/netchesszx_ux_setup_test.exe: tests/spectrum/test_ux_setup.c tests/spectrum/setup_fixture.h src/spectrum/ui/setup.c $(SPECTRUM_HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_HOST_SESSION_TEST -D__z88dk_fastcall= -include tests/session/host_overlay_context.h $< src/spectrum/ui/setup.c -o $@

ux-setup-test: $(BUILD_DIR)/netchesszx_ux_setup_test.exe
	./$(BUILD_DIR)/netchesszx_ux_setup_test.exe

test: ux-setup-test

gui-timer-test: $(GUI_TIMER_TEST) $(GUI_TIMER_60_TEST)
	./$(GUI_TIMER_TEST)
	./$(GUI_TIMER_60_TEST)

$(FILEUI_POLICY_TEST): $(FILEUI_POLICY_TEST_SRC) src/spectrum/overlay/fileui_ovl.c src/spectrum/ui/layout.h $(UI_LAYOUT_INPUTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -D__z88dk_fastcall= -Wno-int-to-pointer-cast \
		-Wno-pointer-to-int-cast \
		$(HOST_INCLUDED_C_CFLAGS) $(HOST_GC_CFLAGS) $(FILEUI_POLICY_TEST_SRC) \
		$(HOST_GC_LDFLAGS) -o $@

fileui-policy-test: $(FILEUI_POLICY_TEST)
	./$(FILEUI_POLICY_TEST)

$(SPECTRANEXT_LINK_TEST): $(SPECTRANEXT_LINK_TEST_SRC) tests/spectrum/spectranext_api_fixture.h src/spectrum/transport/net.c src/spectrum/overlay/direct_ovl.c src/spectrum/overlay/mqtt_connect_ovl.c src/spectrum/overlay/mqtt_tx_ovl.c src/spectrum/overlay/time_ovl.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
		$(HOST_INCLUDED_C_CFLAGS) $(HOST_GC_CFLAGS) $(SPECTRANEXT_LINK_TEST_SRC) \
		$(HOST_GC_LDFLAGS) -o $@

spectranext-network-test: $(SPECTRANEXT_LINK_TEST) tests/tools/test_spectranext_build.py
	./$(SPECTRANEXT_LINK_TEST)
	"$(PYTHON)" tests/tools/test_spectranext_build.py

spectranext-conformance-test: spectranext-network-test $(SESSION_PING_SPECTRANEXT_TEST) $(SESSION_POLL_SPECTRANEXT_TEST) $(SESSION_POLL_SPECTRANEXT_60_TEST) $(SESSION_DIRECT_PARITY_SPECTRANEXT_TEST) $(SESSION_DIRECT_PARITY_SPECTRANEXT_60_TEST) tests/tools/test_spcx_direct_conformance.py
	./$(SESSION_PING_SPECTRANEXT_TEST)
	./$(SESSION_POLL_SPECTRANEXT_TEST)
	./$(SESSION_POLL_SPECTRANEXT_60_TEST)
	./$(SESSION_DIRECT_PARITY_SPECTRANEXT_TEST)
	./$(SESSION_DIRECT_PARITY_SPECTRANEXT_60_TEST)
	"$(PYTHON)" -m unittest tests.tools.test_spcx_direct_conformance

spectranext-clock-test: $(SPECTRANEXT_LINK_TEST)
	./$(SPECTRANEXT_LINK_TEST)

ux-boundaries-check:
	"$(PYTHON)" tools/check_ux_boundaries.py
	"$(PYTHON)" tests/tools/test_ux_boundaries.py
	"$(PYTHON)" tools/check_ux_asm_boundaries.py
	"$(PYTHON)" tests/tools/test_ux_asm_boundaries.py

module-guards: layering-check overlay-cap-check overlay-entry-abi-check transport-contract-check mqtt-client-id-check pc-direct-policy-check spectrum-direct-policy-check session-boundaries-check ux-boundaries-check

layering-check: tools/check_layering.py docs/overlay_state_allowlist.json
	"$(PYTHON)" tools/check_layering.py --root . --overlay-state-allowlist docs/overlay_state_allowlist.json

layering-report: tools/check_layering.py docs/overlay_state_allowlist.json
	"$(PYTHON)" tools/check_layering.py --root . --overlay-state-allowlist docs/overlay_state_allowlist.json --report

overlay-cap-check: tools/check_overlay_caps.py docs/overlay_capabilities.json tools/gen_overlay_defs.py tests/tools/test_overlay_caps.py
	"$(PYTHON)" tools/check_overlay_caps.py --root . --policy docs/overlay_capabilities.json
	"$(PYTHON)" tests/tools/test_overlay_caps.py

overlay-cap-report: tools/check_overlay_caps.py docs/overlay_capabilities.json tools/gen_overlay_defs.py
	"$(PYTHON)" tools/check_overlay_caps.py --root . --policy docs/overlay_capabilities.json --report

overlay-entry-abi-check: tools/check_overlay_entry_abi.py
	"$(PYTHON)" tools/check_overlay_entry_abi.py --root .

transport-contract-check: tools/check_transport_contract.py src/spectrum/transport/link.h src/spectrum/transport/net.c
	"$(PYTHON)" tools/check_transport_contract.py --root .

mqtt-client-id-check: tools/check_mqtt_client_id.py src/pc/client/main_window.cpp src/spectrum/config/session.h src/spectrum/session/identity.h src/spectrum/session/state.h asm/overlay/mqtt_connect/entry_mqtt_connect.asm
	"$(PYTHON)" tools/check_mqtt_client_id.py --root .

pc-direct-policy-check: tools/check_pc_direct_policy.py
	"$(PYTHON)" tools/check_pc_direct_policy.py --root .

pc-policy-guard: pc-direct-policy-check

spectrum-direct-policy-check: tools/check_spectrum_direct_policy.py
	"$(PYTHON)" tools/check_spectrum_direct_policy.py --root .

session-boundaries-check: tools/check_session_boundaries.py tests/session/mqtt_session_transcripts.h tests/session/mqtt_session_transcripts.c tests/session/test_mqtt_session_parity.c tests/session/direct_parity.h tests/session/test_direct_session_parity.c src/spectrum/session/mqtt_wire.c src/spectrum/session/mqtt_connect_actions_ovl.c src/spectrum/session/mqtt_tx_actions_ovl.c src/common/session/session.c src/common/session/session.h src/pc/client/desktop_session_adapter.cpp
	"$(PYTHON)" tools/check_session_boundaries.py --self-test
	"$(PYTHON)" tools/check_session_boundaries.py --root .

check: module-guards | $(BUILD_DIR) $(RELEASE_DIR)
	@fail=0; \
	for t in $(ZCC) $(Z80ASM) z88dk-appmake grep sed head wc dd "$(PYTHON)"; do \
		command -v "$$t" >/dev/null 2>&1 || { echo "[ERR] Missing tool: $$t"; fail=1; }; \
	done; \
	for f in $(SPECTRUM_SRC) $(SPXN_RESIDENT_C) $(SPXN_RESIDENT_ASM) $(SPXN_CLOCK_C) $(SPXN_CLOCK_HEADERS) $(UART_ASM) $(SHRINK_ASM) $(FRAME_CLOCK_ASM) $(EDIT_FIELD_ASM) $(SAN_ASM) $(SCREEN_ASM) $(EDITOR_STATE_ASM) $(RESIDENT_EXTRA_ASM) asm/spectrum/text.asm $(OVERLAY_LOADER_ASM) $(PIECE_ASM) tools/gen_overlay_defs.py; do \
		test -f "$$f" || { echo "[ERR] Missing source: $$f"; fail=1; }; \
	done; \
	exit $$fail

.PHONY: overlay-header-check
overlay-header-check:
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST \
		-D__z88dk_fastcall= -D__z88dk_callee= -Wno-pointer-to-int-cast \
		-Wno-int-to-pointer-cast -fsyntax-only src/spectrum/overlay/direct_ovl.c \
		src/spectrum/overlay/mqtt_connect_ovl.c src/spectrum/overlay/mqtt_tx_ovl.c
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_NEXT -DNETCHESSZX_NEXT_BANKING \
		-D__z88dk_fastcall= -D__z88dk_callee= -Wno-pointer-to-int-cast \
		-Wno-int-to-pointer-cast -fsyntax-only src/spectrum/overlay/direct_ovl.c \
		src/spectrum/overlay/mqtt_connect_ovl.c src/spectrum/overlay/mqtt_tx_ovl.c
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_SPECTRANEXT \
		-D__z88dk_fastcall= -D__z88dk_callee= -Wno-pointer-to-int-cast \
		-fsyntax-only src/spectrum/overlay/overlay.c src/spectrum/saveload/saveload.c
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST -DNETCHESSZX_SPECTRANEXT \
		-D__z88dk_fastcall= -D__z88dk_callee= -Wno-pointer-to-int-cast \
		-Wno-int-to-pointer-cast -include tests/spectrum/spectranext_api_fixture.h \
		-fsyntax-only src/spectrum/overlay/direct_ovl.c src/spectrum/overlay/mqtt_connect_ovl.c \
		src/spectrum/overlay/mqtt_tx_ovl.c src/spectrum/overlay/time_ovl.c

test: overlay-header-check tests/tools/test_check_layering.py tests/tools/test_classic_boot.py tests/tools/test_frame_clock_im2.py tests/tools/test_frame_time_catchup.py tests/tools/test_session_deadline_z80.py tests/tools/test_editor_append_hot.py tests/tools/test_direct_queue_pop.py tests/tools/test_recovery.py tests/tools/test_recovery_spectranext.py tests/tools/xfs_model.py $(TIME_60_TEST) $(GUI_TIMER_60_TEST) $(SESSION_TRANSITIONS_TEST) $(SESSION_MQTT_PARITY_NEXT_TEST) $(SESSION_MQTT_PARITY_NEXT_60_TEST) $(TIME_RUNTIME_TEST) $(TIME_RUNTIME_60_TEST)
test: $(UART_PLATFORM_NEXT_TEST) $(MQTT_STREAM_NEXT_TEST) $(RULES_TEST) $(MOVE_COORDS_TEST) $(SAVEGAME_WIRE_TEST) $(RECOVERY_CAPTURE_TEST) $(JOURNAL_TEST) $(SAVELOAD_ESXDOS_TEST) $(BOARD_TEST) $(RULES_COMPACT_PERFT_TEST) $(MQTT_TEST) $(MQTT_STREAM_TEST) $(GAME_PROTOCOL_TEST) $(MQTT_SESSION_PROTOCOL_TEST) $(DIRECT_SESSION_PROTOCOL_TEST) $(SESSION_CORE_TEST) $(SESSION_MQTT_PARITY_TEST) $(SESSION_MQTT_PARITY_60_TEST) $(SESSION_DIRECT_CORE_TEST) $(SESSION_DIRECT_PARITY_TEST) $(SESSION_DIRECT_PARITY_NEXT_TEST) $(SESSION_DIRECT_PARITY_NEXT_60_TEST) $(SESSION_SPECTRUM_PAIR_TEST) $(KEEPALIVE_PROTOCOL_TEST) $(ESP_AT_TEST) $(ESP_AT_NEXT_TEST) $(UART_PLATFORM_TEST) $(DIRECT_IPD_TEST) $(SESSION_CONFIG_TEST) $(APP_CONFIG_FORMAT_TEST) $(CONFIG_OVERLAY_TEST) $(SESSION_PING_TEST) $(SESSION_PING_NEXT_TEST) $(SESSION_POLL_TEST) $(SESSION_POLL_60_TEST) $(SESSION_DIRECT_TEST) $(SESSION_EVENT_TEST) $(SESSION_MQTT_TEST) $(SESSION_OUTGOING_TEST) $(STATUS_OVERLAY_TEST) $(GUI_LOG_TEST) $(TIME_TEST) $(GUI_TIMER_TEST) $(FILEUI_POLICY_TEST) tests/tools/test_gen_assets_about.py tests/tools/test_next_flip_sprite_slots.py tests/tools/test_next_graphics_bank.py tests/tools/test_next_overlay_pages.py tests/tools/test_next_board_theme_rgb333.py tests/tools/test_tap_image.py tests/tools/test_build_overlays.py tests/tools/test_spectrum_asm_vectors.py tests/tools/test_journal_hook.py tests/tools/test_journal_link.py tests/tools/test_stj_read.py tests/tools/test_setup_flow.py tests/tools/test_setup_port.py tests/spectrum/rules_asm_vector.asm asm/overlay/rules/rules_stub.asm tests/spectrum/proto_copy_token_vector.asm tests/spectrum/frame_arg_push_vector.asm tests/spectrum/timer_tick_vector.asm tests/spectrum/mqtt_connect_vector.asm tests/spectrum/setup_visible_vector.asm tests/spectrum/edit_field_vector.asm tests/spectrum/edit_buf_vector.asm tests/spectrum/gui_log_notify_vector.asm tests/spectrum/gui_log_chat_vector.asm tests/spectrum/san_vector.asm tests/spectrum/streq_vector.asm
	"$(PYTHON)" -m unittest discover -s tests/tools -p "test_gen_assets_about.py"
	"$(PYTHON)" -m unittest tests.tools.test_direct_burst_probe
	"$(PYTHON)" tests/tools/test_check_layering.py
	"$(PYTHON)" tests/tools/test_tap_image.py
	"$(PYTHON)" tests/tools/test_classic_boot.py
	"$(PYTHON)" tests/tools/test_graphics_layout.py
	"$(PYTHON)" tests/tools/test_build_overlays.py
	"$(PYTHON)" tests/tools/test_next_flip_sprite_slots.py
	"$(PYTHON)" tests/tools/test_next_graphics_bank.py
	"$(PYTHON)" tests/tools/test_next_overlay_pages.py
	"$(PYTHON)" tests/tools/test_next_board_theme_rgb333.py
	"$(PYTHON)" tests/tools/test_spectrum_asm_vectors.py
	"$(PYTHON)" tests/tools/test_game_stats_z80.py
	"$(PYTHON)" tests/tools/test_game_stats_erase.py
	"$(PYTHON)" tests/tools/test_game_stats_render.py
	"$(PYTHON)" tests/tools/test_journal_hook.py
	"$(PYTHON)" tests/tools/test_journal_link.py
	"$(PYTHON)" tests/tools/test_recovery.py
	"$(PYTHON)" tests/tools/test_recovery_spectranext.py
	"$(PYTHON)" tests/tools/test_stj_read.py
	"$(PYTHON)" tests/tools/test_timer_roll_start.py
	"$(PYTHON)" tests/tools/test_frame_clock_im2.py
	"$(PYTHON)" tests/tools/test_frame_time_catchup.py
	"$(PYTHON)" tests/tools/test_session_deadline_z80.py
	"$(PYTHON)" tests/tools/test_editor_append_hot.py
	"$(PYTHON)" tests/tools/test_direct_queue_pop.py
	"$(PYTHON)" tests/tools/test_classic_overlay_dispatch.py
	"$(PYTHON)" tests/tools/test_spectranext_paging.py
	"$(PYTHON)" -c "import runpy; [f() for p in ('tests/tools/test_setup_flow.py','tests/tools/test_setup_port.py') for n,f in sorted(runpy.run_path(p).items()) if n.startswith('test_')]"
	./$(RULES_TEST)
	./$(MOVE_COORDS_TEST)
	./$(SAVEGAME_WIRE_TEST)
	./$(RECOVERY_CAPTURE_TEST)
	./$(JOURNAL_TEST)
	./$(SAVELOAD_ESXDOS_TEST)
	./$(BOARD_TEST)
	./$(RULES_COMPACT_PERFT_TEST)
	./$(MQTT_TEST)
	./$(MQTT_STREAM_TEST)
	./$(ESP_AT_TEST)
	./$(ESP_AT_NEXT_TEST)
	./$(UART_PLATFORM_TEST)
	./$(UART_PLATFORM_NEXT_TEST)
	./$(MQTT_STREAM_NEXT_TEST)
	./$(DIRECT_IPD_TEST)
	./$(GAME_PROTOCOL_TEST)
	./$(MQTT_SESSION_PROTOCOL_TEST)
	./$(DIRECT_SESSION_PROTOCOL_TEST)
	./$(SESSION_CORE_TEST)
	./$(SESSION_MQTT_PARITY_TEST)
	./$(SESSION_MQTT_PARITY_60_TEST)
	./$(SESSION_MQTT_PARITY_NEXT_TEST)
	./$(SESSION_MQTT_PARITY_NEXT_60_TEST)
	./$(SESSION_DIRECT_CORE_TEST)
	./$(SESSION_DIRECT_PARITY_TEST)
	./$(SESSION_DIRECT_PARITY_NEXT_TEST)
	./$(SESSION_DIRECT_PARITY_NEXT_60_TEST)
	./$(SESSION_SPECTRUM_PAIR_TEST)
	./$(KEEPALIVE_PROTOCOL_TEST)
	./$(SESSION_CONFIG_TEST)
	./$(APP_CONFIG_FORMAT_TEST)
	./$(CONFIG_OVERLAY_TEST)
	./$(SESSION_PING_TEST)
	./$(SESSION_PING_NEXT_TEST)
	./$(SESSION_POLL_TEST)
	./$(MQTT_CONNECT_ACTIONS_TEST)
	./$(SESSION_POLL_60_TEST)
	./$(SESSION_DIRECT_TEST)
	./$(SESSION_EVENT_TEST)
	./$(SESSION_MQTT_TEST)
	./$(SESSION_OUTGOING_TEST)
	./$(STATUS_OVERLAY_TEST)
	./$(GUI_LOG_TEST)
	./$(TIME_TEST)
	./$(SESSION_TRANSITIONS_TEST)
	./$(GUI_TIMER_TEST)
	./$(TIME_60_TEST)
	./$(GUI_TIMER_60_TEST)
	./$(TIME_RUNTIME_TEST)
	./$(TIME_RUNTIME_60_TEST)
	./$(FILEUI_POLICY_TEST)

rules-oracle: $(RULES_COMPACT_PERFT_TEST)
	"$(PYTHON)" tests/spectrum/rules_compact_pychess_oracle.py $(RULES_ORACLE_ARGS)

spectrum-rules-asm-test: $(RULES_COMPACT_PERFT_TEST) tests/tools/test_spectrum_asm_vectors.py tests/spectrum/rules_asm_vector.asm asm/overlay/rules/rules_stub.asm
	"$(PYTHON)" -c "import runpy; runpy.run_path('tests/tools/test_spectrum_asm_vectors.py')['run_rules_asm_vector']()"

rules-deep: $(RULES_COMPACT_PERFT_TEST)
	NETCHESSZX_DEEP_PERFT=1 ./$(RULES_COMPACT_PERFT_TEST)

session-spectrum-pair-test: $(SESSION_SPECTRUM_PAIR_TEST)
	./$(SESSION_SPECTRUM_PAIR_TEST)

tap: $(ZX_TAP)
tap: $(ZX_OVL)
tap: $(ZX_DAT)
tap: move-listings

# zcc drops .c.asm listings next to the sources; sweep them into
# $(BUILD_DIR)/listings so src/ stays clean (shrink sessions read them there).
move-listings: $(ZX_TAP) $(ZX_OVL)
	@mkdir -p $(BUILD_DIR)/listings
	@find src -name '*.c.asm' -exec sh -c 'mkdir -p "$(BUILD_DIR)/listings/$$(dirname "$$1")" && mv -f "$$1" "$(BUILD_DIR)/listings/$$1"' _ {} \;

.PHONY: move-listings

tap-divmmc:
	$(MAKE) NET_BACKEND=esp_at FS_BACKEND=esxdos UART_BACKEND=divmmc tap

tap-spectranext:
	$(MAKE) NET_BACKEND=spectranext FS_BACKEND=xfs UART_BACKEND=none \
		ZX_EXTRA_CFLAGS='$(ZX_EXTRA_CFLAGS)' \
		ATLAS_SEED_PROFILE=spectranext ATLAS_EXTRA='TIME RECOVERY' \
		JOURNAL_HOOK=1 STJ_SAVELOAD=1 \
		OVERLAY_BLOCK_SIZE=4096 OVERLAY_SIZE_LIMIT=4096 \
		BUILD_DIR=$(BUILD_DIR)/spectranext RELEASE_DIR=$(RELEASE_DIR)/Spectranext \
		LOWMEM_LAYOUT=spectranext tap

spectranext-size-report: tap-spectranext tools/gen_size_report.py
	"$(PYTHON)" tools/gen_size_report.py --map $(BUILD_DIR)/spectranext/$(ZX_NAME).map --code-bin $(BUILD_DIR)/spectranext/$(ZX_NAME)_CODE.bin --tap $(RELEASE_DIR)/Spectranext/$(ZX_NAME).tap --ovl $(RELEASE_DIR)/Spectranext/$(ZX_NAME).OVL --dat $(RELEASE_DIR)/Spectranext/$(ZX_NAME).DAT --overlay-sizes $(BUILD_DIR)/spectranext/overlay_sizes.json --output $(BUILD_DIR)/spectranext/size_report.json

spectranext-port-build: spectranext-network-test spectranext-config-test spectranext-storage-test spectranext-clock-test spectranext-size-report
	@set -eu; \
		out="$(BUILD_DIR)/spectranext-resource"; \
		stage="$$out.stage"; \
		previous="$$out.previous"; \
		rm -rf "$$stage"; \
		"$(PYTHON)" $(abspath $(SPXN_DIR)/../tools/dev.py) installer \
			packaging/spectranext/installer.json "$$stage" \
			--preview build/installer-preview.png; \
		rm -rf "$$previous"; \
		had_previous=0; \
		if [ -d "$$out" ]; then mv "$$out" "$$previous"; had_previous=1; fi; \
		if mv "$$stage" "$$out"; then \
			rm -rf "$$previous"; \
		else \
			if [ "$$had_previous" = 1 ]; then mv "$$previous" "$$out"; fi; \
			exit 1; \
		fi

# Direct HTTP(S) launch folder (build/spectranext-direct); builds the Spectranext TAP, publishes nothing.
spectranext-direct: tap-spectranext
	@"$(PYTHON)" tools/build_spectranext_direct.py "$(SPXN_DIR)"
	@"$(PYTHON)" tests/tools/test_spectranext_direct.py

# GitHub Pages site (build/spectranext-pages): direct launch at the root with
# the landing page, installer under install/. Publishes nothing.
spectranext-pages: spectranext-port-build spectranext-direct
	@"$(PYTHON)" tools/build_spectranext_pages.py
	@"$(PYTHON)" tests/tools/test_spectranext_pages.py

next-size-report: tools/gen_size_report.py
	$(MAKE) NET_BACKEND=esp_at FS_BACKEND=esxdos UART_BACKEND=next ZX_TARGET_CFLAGS='-DNETCHESSZX_NEXT' ATLAS_SEED_PROFILE=next-tap ATLAS_EXTRA=RECOVERY BUILD_DIR=$(BUILD_DIR)/next RELEASE_DIR=$(BUILD_DIR)/next/stage tap
	"$(PYTHON)" tools/gen_size_report.py --map $(BUILD_DIR)/next/$(ZX_NAME).map --code-bin $(BUILD_DIR)/next/$(ZX_NAME)_CODE.bin --tap $(BUILD_DIR)/next/stage/$(ZX_NAME).tap --ovl $(BUILD_DIR)/next/stage/$(ZX_NAME).OVL --dat $(BUILD_DIR)/next/stage/$(ZX_NAME).DAT --overlay-sizes $(BUILD_DIR)/next/overlay_sizes.json --output $(BUILD_DIR)/next/size_report.json

nex: $(ZX_NEX)

$(NEXT_SPRITE_BIN) $(NEXT_SPRITE_PALETTE_ASM) $(NEXT_SPRITE_PAL_BIN) $(NEXT_SPRITE_META): tools/build_next_piece_sprites.py assets/lichess/selected_next_sets.json assets/lichess/selected_next_boards.json $(NEXT_SPRITE_SOURCE_ASSETS)
	"$(PYTHON)" tools/build_next_piece_sprites.py

$(NEXT_NEX_CONFIG_STAMP): FORCE tools/write_build_config.py Makefile
	@"$(PYTHON)" tools/write_build_config.py --out $@ \
		--value "PORT=$(PORT)" \
		--value "MQTT_HOST=$(MQTT_HOST)" \
		--value "MQTT_PORT=$(MQTT_PORT)" \
		--value "MQTT_CODE=$(MQTT_CODE)" \
		--value "ZX_EXTRA_CFLAGS=$(ZX_EXTRA_CFLAGS)" \
		--value "ZCC=$(ZCC)" \
		--value "ZCC_ID=$(ZCC_ID)" \
		--value "Z80ASM=$(Z80ASM)" \
		--value "Z80ASM_ID=$(Z80ASM_ID)" \
		--value "NEXT_ZX0=$(NEXT_ZX0)" \
		--value "NEXT_ZX_ORG=$(NEXT_ZX_ORG)" \
		--value "NEXT_EXTENSION_FREE_MIN=$(NEXT_EXTENSION_FREE_MIN)" \
		--value "NEXT_MIN_SP_GAP=$(NEXT_MIN_SP_GAP)"

$(ZX_NEX): $(NEXT_NEX_CONFIG_STAMP) $(NEXT_NEX_INPUTS) tools/gen_next_nex.py tools/gen_next_graphics_defs.py tools/next_bundle_codec.py tests/tools/test_next_bundle_codec.py tests/tools/test_next_graphics_bank.py tests/tools/test_next_overlay_pages.py tests/tools/test_next_paging.py tests/tools/test_next_board_theme_rgb333.py tools/make_about_nxi.py $(ASM_DATA_TOOL) $(NEXT_GRAPHICS_BANK_ASM) $(NEXT_GRAPHICS_BANK_LAYOUT) $(NEXT_EXTENSION_INCLUDES) $(NEXT_SPRITE_BIN) $(NEXT_SPRITE_PAL_BIN) $(NEXT_ABOUT_NXI_SRC)
	$(MAKE) NET_BACKEND=esp_at FS_BACKEND=esxdos UART_BACKEND=next OVERLAY_LOADER_ASM=$(NEXT_OVERLAY_LOADER_ASM) RESIDENT_EXTRA_ASM=$(NEXT_RESIDENT_ASM) RESIDENT_INCLUDE_DEPS='$(RESIDENT_INCLUDE_DEPS) $(NEXT_GRAPHICS_BANK_LAYOUT) $(NEXT_EXTENSION_INCLUDES)' SCREEN_ASM=asm/next/screen.asm ZX_ORG=$(NEXT_ZX_ORG) OVERLAY_BLOCK_SIZE=8192 OVERLAY_SIZE_LIMIT=4096 ZX_TARGET_CFLAGS='-DNETCHESSZX_NEXT -DNETCHESSZX_NEXT_BANKING -Ca-DNETCHESSZX_NEXT -Ca-DNETCHESSZX_NEXT_BANKING -Ca-DNETCHESSZX_NEXT_RESIDENT' ATLAS_SEED_PROFILE=next ATLAS_EXTRA=RECOVERY JOURNAL_HOOK=1 STJ_SAVELOAD=1 BUILD_DIR=$(NEXT_NEX_BUILD_DIR) RELEASE_DIR=$(NEXT_NEX_STAGE_DIR) LOWMEM_LAYOUT=next tap
	"$(PYTHON)" tools/make_about_nxi.py $(NEXT_ABOUT_NXI_SRC) $(NEXT_UI_ASSET_ASM) $(NEXT_ABOUT_NXI)
	"$(PYTHON)" tests/tools/test_next_bundle_codec.py --zx0 "$(NEXT_ZX0)"
	@if [ "$(NEXT_STATIC_TESTS_DONE)" != "1" ]; then \
		"$(PYTHON)" tests/tools/test_next_graphics_bank.py && \
		"$(PYTHON)" tests/tools/test_next_overlay_pages.py && \
		"$(PYTHON)" tests/tools/test_next_board_theme_rgb333.py; \
	fi
	"$(PYTHON)" tools/gen_next_graphics_defs.py $(NEXT_NEX_BUILD_DIR)/$(ZX_NAME).map > $(NEXT_GRAPHICS_BANK_DEFS)
	rm -f $(NEXT_GRAPHICS_BANK_DEFS:.asm=.o) $(NEXT_GRAPHICS_BANK_OBJ)
	$(ZX_Z80ASM) -DNETCHESSZX_NEXT -DNETCHESSZX_NEXT_EXTENSION -I=$(NEXT_NEX_BUILD_DIR) -O=$(NEXT_NEX_BUILD_DIR) $(NEXT_GRAPHICS_BANK_ASM)
	$(ZX_Z80ASM) -b -r$(NEXT_GRAPHICS_BANK_ORG) -o=$(NEXT_GRAPHICS_BANK_BIN) $(NEXT_GRAPHICS_BANK_OBJ)
	@graphics_size=$$(wc -c < $(NEXT_GRAPHICS_BANK_BIN)); \
	graphics_max=$$(( $(NEXT_GRAPHICS_BANK_LIMIT) - $(NEXT_EXTENSION_FREE_MIN) )); \
	if [ "$$graphics_size" -gt "$$graphics_max" ]; then \
		printf "[ERR] Next extension bank too large: $$graphics_size bytes (max $$graphics_max; $(NEXT_EXTENSION_FREE_MIN) free required)\n"; \
		exit 1; \
	fi; \
	graphics_free=$$(( $(NEXT_GRAPHICS_BANK_LIMIT) - graphics_size )); \
	printf "[OK] Next extension bank: $$graphics_size/$(NEXT_GRAPHICS_BANK_LIMIT) bytes ($$graphics_free free)\n"
	"$(PYTHON)" tools/gen_next_nex.py --map $(NEXT_NEX_BUILD_DIR)/$(ZX_NAME).map --code-bin $(NEXT_NEX_BUILD_DIR)/$(ZX_NAME)_CODE.bin --ovl $(NEXT_NEX_STAGE_DIR)/$(ZX_NAME).OVL --overlay-offset $(NEXT_OVERLAY_OFFSET) --dat $(NEXT_NEX_STAGE_DIR)/$(ZX_NAME).DAT --out $(ZX_NEX) --org $(NEXT_ZX_ORG) --bundle-bank-base $(NEXT_BUNDLE_BANK_BASE) --raw-bank-base $(NEXT_RAW_BANK_BASE) --max-bundle-banks $(NEXT_MAX_BUNDLE_BANKS) --zx0 "$(NEXT_ZX0)" --dat-offset $(NEXT_BUNDLE_DAT_OFFSET) --sprite-patterns $(NEXT_SPRITE_BIN) --sprite-offset $(NEXT_SPRITE_OFFSET) --sprite-pal $(NEXT_SPRITE_PAL_BIN) --sprite-pal-offset $(NEXT_SPRITE_PAL_OFFSET) --about-nxi $(NEXT_ABOUT_NXI) --about-pal-offset $(NEXT_ABOUT_PAL_OFFSET) --about-pixels-offset $(NEXT_ABOUT_PIXELS_OFFSET) --graphics-bank $(NEXT_GRAPHICS_BANK_BIN) --graphics-bank-offset $(NEXT_GRAPHICS_BANK_OFFSET) --graphics-bank-org $(NEXT_GRAPHICS_BANK_ORG)

nex-size-report: nex tools/gen_size_report.py
	"$(PYTHON)" tools/gen_size_report.py --map $(NEXT_NEX_BUILD_DIR)/$(ZX_NAME).map --code-bin $(NEXT_NEX_BUILD_DIR)/$(ZX_NAME)_CODE.bin --tap $(NEXT_NEX_STAGE_DIR)/$(ZX_NAME).tap --ovl $(NEXT_NEX_STAGE_DIR)/$(ZX_NAME).OVL --dat $(NEXT_NEX_STAGE_DIR)/$(ZX_NAME).DAT --overlay-sizes $(NEXT_NEX_BUILD_DIR)/overlay_sizes.json --output $(NEXT_SIZE_REPORT) --org $(NEXT_ZX_ORG) --min-sp-gap $(NEXT_MIN_SP_GAP)

next: nex

tap-direct-overlay:
	$(MAKE) tap

sdcc-iy-contract-check: tools/check_sdcc_iy_contract.py | $(BUILD_DIR)
	"$(PYTHON)" tools/check_sdcc_iy_contract.py --zcc "$(ZCC)" --build-dir "$(BUILD_DIR)"

abi-next-manifest: nex tools/gen_abi_manifest.py tools/gen_overlay_defs.py $(ABI_HEADERS)
	"$(PYTHON)" tools/gen_abi_manifest.py $(NEXT_NEX_BUILD_DIR)/$(ZX_NAME).map --target next --output $(NEXT_ABI_MANIFEST)

abi-manifest: abi-next-manifest tap tools/gen_abi_manifest.py tools/gen_overlay_defs.py $(ABI_HEADERS) | $(BUILD_DIR)
	"$(PYTHON)" tools/gen_abi_manifest.py $(BUILD_DIR)/$(ZX_NAME).map --target classic --output $(ABI_MANIFEST)

abi-next-baseline: nex tools/gen_abi_manifest.py tools/gen_overlay_defs.py $(ABI_HEADERS)
	"$(PYTHON)" tools/gen_abi_manifest.py $(NEXT_NEX_BUILD_DIR)/$(ZX_NAME).map --target next --output $(NEXT_ABI_MANIFEST) --write-baseline $(NEXT_ABI_BASELINE)

abi-baseline: abi-next-baseline tap tools/gen_abi_manifest.py tools/gen_overlay_defs.py $(ABI_HEADERS) | $(BUILD_DIR)
	"$(PYTHON)" tools/gen_abi_manifest.py $(BUILD_DIR)/$(ZX_NAME).map --target classic --output $(ABI_MANIFEST) --write-baseline $(ABI_BASELINE)

abi-next-check: nex tools/gen_abi_manifest.py tools/gen_overlay_defs.py $(ABI_HEADERS)
	"$(PYTHON)" tools/gen_abi_manifest.py $(NEXT_NEX_BUILD_DIR)/$(ZX_NAME).map --target next --output $(NEXT_ABI_MANIFEST) --baseline $(NEXT_ABI_BASELINE) --fail-on-missing-baseline $(ABI_CHECK_FLAGS)

abi-check: abi-next-check tap tools/gen_abi_manifest.py tools/gen_overlay_defs.py $(ABI_HEADERS) | $(BUILD_DIR)
	"$(PYTHON)" tools/gen_abi_manifest.py $(BUILD_DIR)/$(ZX_NAME).map --target classic --output $(ABI_MANIFEST) --baseline $(ABI_BASELINE) --fail-on-missing-baseline $(ABI_CHECK_FLAGS)

size-report: tap tools/gen_size_report.py | $(BUILD_DIR)
	"$(PYTHON)" tools/gen_size_report.py --map $(BUILD_DIR)/$(ZX_NAME).map --code-bin $(BUILD_DIR)/$(ZX_NAME)_CODE.bin --tap $(ZX_TAP) --ovl $(ZX_OVL) --dat $(ZX_DAT) --overlay-sizes $(BUILD_DIR)/overlay_sizes.json --output $(SIZE_REPORT)

size-baseline: tap tools/gen_size_report.py | $(BUILD_DIR)
	"$(PYTHON)" tools/gen_size_report.py --map $(BUILD_DIR)/$(ZX_NAME).map --code-bin $(BUILD_DIR)/$(ZX_NAME)_CODE.bin --tap $(ZX_TAP) --ovl $(ZX_OVL) --dat $(ZX_DAT) --overlay-sizes $(BUILD_DIR)/overlay_sizes.json --output $(SIZE_REPORT) --write-baseline $(SIZE_BASELINE)

size-check: tap tools/gen_size_report.py | $(BUILD_DIR)
	"$(PYTHON)" tools/gen_size_report.py --map $(BUILD_DIR)/$(ZX_NAME).map --code-bin $(BUILD_DIR)/$(ZX_NAME)_CODE.bin --tap $(ZX_TAP) --ovl $(ZX_OVL) --dat $(ZX_DAT) --overlay-sizes $(BUILD_DIR)/overlay_sizes.json --output $(SIZE_REPORT) --baseline $(SIZE_BASELINE) --fail-on-missing-baseline $(SIZE_CHECK_FLAGS)

# One overlay against an existing Classic/Next/SpectraNext map. Does not
# relink TAP or pack the atlas. OVERLAY is the atlas name (SETUP, GUI_LOG, ...).
# Refresh the config stamp first; the builder refuses a stamp newer than the map.
OVERLAY ?=
overlay-size: $(BUILD_CONFIG_STAMP) $(UI_LAYOUT_ASM)
	"$(PYTHON)" tools/build_overlays.py --only "$(OVERLAY)" --size-check --baseline "$(SIZE_BASELINE)" $(SIZE_CHECK_FLAGS)

# Not named `release`: that is the $(RELEASE_DIR) output directory rule.
# The size baseline is measured at the default allocation budget, so growth
# against it is only reported here; caps, SP floors and ABI still fail.
release-check:
	@$(MAKE) MAX_ALLOCS_PER_NODE=200000 SIZE_CHECK_FLAGS= full-check

.PHONY: release-check

full-check: NEXT_STATIC_TESTS_DONE := 1
full-check: SIZE_CHECK_FLAGS := --fail-on-growth
full-check: module-guards test abi-check size-check nex-size-report
	"$(PYTHON)" tests/tools/test_board_action_z80.py $(BUILD_DIR)
	"$(PYTHON)" tests/tools/test_board_action_z80.py $(NEXT_NEX_BUILD_DIR) --next
	"$(PYTHON)" tests/tools/test_editor_append_hot.py --linked-only \
		--linked-build $(BUILD_DIR) --linked-build $(NEXT_NEX_BUILD_DIR)
	"$(PYTHON)" tests/tools/test_live_im1_iy.py $(BUILD_DIR) $(NEXT_NEX_BUILD_DIR)

literal-report: tools/gen_literal_report.py | $(BUILD_DIR)
	"$(PYTHON)" tools/gen_literal_report.py --root . --output $(LITERAL_REPORT)

client:
ifeq ($(CLIENT_BUILD),auto)
ifneq ($(CLIENT_HOST_IS_WINDOWS),)
	@$(MAKE) client-msvc
else
	@$(MAKE) client-cmake
endif
else
	@$(MAKE) client-$(CLIENT_BUILD)
endif

# Shared desktop dev loop. Windows preserves its external MSVC build tree;
# macOS/Linux use the common CMake tree under build/qt-client.
client-test:
ifneq ($(CLIENT_HOST_IS_WINDOWS),)
	@command -v "$(POWERSHELL)" >/dev/null 2>&1 || { echo "[ERR] Missing PowerShell 7: set POWERSHELL=pwsh or use CLIENT_BUILD=cmake"; exit 1; }
	$(POWERSHELL) -NoLogo -NoProfile -ExecutionPolicy Bypass -File client/build-dev.ps1
else
	@command -v "$(CMAKE)" >/dev/null 2>&1 || { echo "[ERR] Missing cmake: set CMAKE=/path/to/cmake"; exit 1; }
	@command -v "$(CTEST)" >/dev/null 2>&1 || { echo "[ERR] Missing ctest: set CTEST=/path/to/ctest"; exit 1; }
	$(CMAKE) -S client -B "$(CLIENT_CMAKE_BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CLIENT_CMAKE_CONFIG)" -DBUILD_TESTING=ON $(CLIENT_CMAKE_ARGS)
	$(CMAKE) --build "$(CLIENT_CMAKE_BUILD_DIR)" --config "$(CLIENT_CMAKE_CONFIG)" --parallel
	$(CTEST) --test-dir "$(CLIENT_CMAKE_BUILD_DIR)" -C "$(CLIENT_CMAKE_CONFIG)" --output-on-failure
endif

client-msvc:
	@command -v "$(POWERSHELL)" >/dev/null 2>&1 || { echo "[ERR] Missing PowerShell 7: set POWERSHELL=pwsh or use CLIENT_BUILD=cmake"; exit 1; }
	$(POWERSHELL) -NoLogo -NoProfile -ExecutionPolicy Bypass -File client/build-msvc.ps1 -QtDir "$(CLIENT_MSVC_QT_DIR)" -Config "$(CLIENT_MSVC_CONFIG)" -Generator "$(CLIENT_MSVC_GENERATOR)" -Architecture "$(CLIENT_MSVC_ARCH)" -CleanTimeoutSeconds $(CLIENT_MSVC_CLEAN_TIMEOUT)

client-cmake:
	@command -v "$(CMAKE)" >/dev/null 2>&1 || { echo "[ERR] Missing cmake: set CMAKE=/path/to/cmake"; exit 1; }
	$(CMAKE) -S client -B "$(CLIENT_CMAKE_BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CLIENT_CMAKE_CONFIG)" $(CLIENT_CMAKE_ARGS)
ifeq ($(HOST_UNAME),Darwin)
	$(CMAKE) -E rm -rf "$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app"
endif
	$(CMAKE) --build "$(CLIENT_CMAKE_BUILD_DIR)" --config "$(CLIENT_CMAKE_CONFIG)" --parallel
ifeq ($(HOST_UNAME),Darwin)
	@command -v "$(MACDEPLOYQT)" >/dev/null 2>&1 || { echo "[ERR] Missing macdeployqt: set MACDEPLOYQT=/path/to/macdeployqt"; exit 1; }
	@command -v "$(CODESIGN)" >/dev/null 2>&1 || { echo "[ERR] Missing codesign: set CODESIGN=/path/to/codesign"; exit 1; }
	@command -v "$(DITTO)" >/dev/null 2>&1 || { echo "[ERR] Missing ditto: set DITTO=/path/to/ditto"; exit 1; }
	@command -v "$(QTPATHS)" >/dev/null 2>&1 || { echo "[ERR] Missing qtpaths: set QTPATHS=/path/to/qtpaths"; exit 1; }
	$(MACDEPLOYQT) "$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app" -always-overwrite -no-codesign -no-plugins
	$(CMAKE) -E make_directory \
		"$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/platforms" \
		"$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/styles" \
		"$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/imageformats" \
		"$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/tls"
	$(CMAKE) -E copy_if_different "$$($(QTPATHS) --plugin-dir)/platforms/libqcocoa.dylib" "$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/platforms/libqcocoa.dylib"
	$(CMAKE) -E copy_if_different "$$($(QTPATHS) --plugin-dir)/styles/libqmacstyle.dylib" "$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/styles/libqmacstyle.dylib"
	$(CMAKE) -E copy_if_different "$$($(QTPATHS) --plugin-dir)/imageformats/libqjpeg.dylib" "$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/imageformats/libqjpeg.dylib"
	# HTTPS (Stockfish download) needs a Qt TLS backend; SecureTransport is native.
	$(CMAKE) -E copy_if_different "$$($(QTPATHS) --plugin-dir)/tls/libqsecuretransportbackend.dylib" "$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/tls/libqsecuretransportbackend.dylib"
	$(MACDEPLOYQT) "$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app" -always-overwrite -no-codesign -no-plugins \
		-executable="$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/platforms/libqcocoa.dylib" \
		-executable="$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/styles/libqmacstyle.dylib" \
		-executable="$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/imageformats/libqjpeg.dylib" \
		-executable="$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app/Contents/PlugIns/tls/libqsecuretransportbackend.dylib"
	$(CODESIGN) --force --deep --sign - "$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app"
	$(CODESIGN) --verify --deep --strict "$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app"
	$(CMAKE) -E make_directory "$(CLIENT_MAC_APPLICATIONS_DIR)"
	$(CMAKE) -E rm -rf "$(CLIENT_MAC_APPLICATIONS_DIR)/Shatranj.app"
	$(DITTO) "$(CLIENT_CMAKE_BUILD_DIR)/shatranj-client.app" "$(CLIENT_MAC_APPLICATIONS_DIR)/Shatranj.app"
endif


$(RELEASE_DIR):
	mkdir -p $(RELEASE_DIR)

$(PIECE_ASM): tools/build_piece_sets.py $(ASM_DATA_TOOL) $(PIECE_PNGS)
	"$(PYTHON)" tools/build_piece_sets.py

$(ABOUT_BOARD): tools/make_about_board.py $(ASM_DATA_TOOL) $(ABOUT_BOARD_SRC) assets/spectrum/ui_runtime_assets.asm
	"$(PYTHON)" tools/make_about_board.py $(ABOUT_BOARD_SRC) assets/spectrum/ui_runtime_assets.asm $(ABOUT_BOARD)

$(UI_LAYOUT_ASM): $(UI_LAYOUT_INPUTS) | $(BUILD_DIR)
	"$(PYTHON)" tools/gen_ui_layout.py $(UI_LAYOUT_HEADER) $@

$(BUILD_DAT): $(BUILD_CONFIG_STAMP) tools/gen_assets.py tools/next_bundle_codec.py $(ASM_DATA_TOOL) $(ASSET_ASM) $(PIECE_ASM) $(ABOUT_BOARD) $(SCREEN_ASM) $(OVERLAY_LOADER_ASM) $(DAT_PLATFORM_DEPS) VERSION | $(BUILD_DIR)
	"$(PYTHON)" tools/gen_assets.py $(ASSET_ASM) $(PIECE_ASM) $(SCREEN_ASM) $(OVERLAY_LOADER_ASM) $(ABOUT_BOARD) $(BUILD_DAT) --version $(APP_VERSION) --zx0 "$(ZX0)" $(DAT_PLATFORM_FLAG)

$(ZX_DAT): $(BUILD_DAT) | $(RELEASE_DIR)
	cp -f $(BUILD_DAT) $(ZX_DAT)

$(BUILD_CONFIG_STAMP): FORCE tools/write_build_config.py Makefile | $(BUILD_DIR)
	@"$(PYTHON)" tools/write_build_config.py --out $@ \
		--value "PORT=$(PORT)" \
		--value "MQTT_HOST=$(MQTT_HOST)" \
		--value "MQTT_PORT=$(MQTT_PORT)" \
		--value "MQTT_CODE=$(MQTT_CODE)" \
		--value "TIME_HOST=$(TIME_HOST)" \
		--value "UART_BACKEND=$(UART_BACKEND)" \
		--value "NET_BACKEND=$(NET_BACKEND)" \
		--value "FS_BACKEND=$(FS_BACKEND)" \
		--value "SPXN_DIR=$(SPXN_DIR_ABS)" \
		--value "SCREEN_ASM=$(SCREEN_ASM)" \
		--value "OVERLAY_LOADER_ASM=$(OVERLAY_LOADER_ASM)" \
		--value "OVERLAY_BLOCK_SIZE=$(OVERLAY_BLOCK_SIZE)" \
		--value "OVERLAY_SIZE_LIMIT=$(OVERLAY_SIZE_LIMIT)" \
		--value "RESIDENT_EXTRA_ASM=$(RESIDENT_EXTRA_ASM)" \
		--value "ZCC=$(ZCC)" \
		--value "ZCC_ID=$(ZCC_ID)" \
		--value "Z80ASM=$(Z80ASM)" \
		--value "Z80ASM_ID=$(Z80ASM_ID)" \
		--value "ZX0=$(ZX0)" \
		--value "ZX_CFLAGS=$(ZX_CFLAGS)" \
		--value "ZX_OVL_CFLAGS=$(ZX_OVL_CFLAGS)" \
		--value "MQTT_CONNECT_OVL_CFLAGS=$(MQTT_CONNECT_OVL_CFLAGS)" \
		--value "CONFIG_OVL_CFLAGS=$(CONFIG_OVL_CFLAGS)" \
		--value "SPXN_CLOCK_OVL_CFLAGS=$(SPXN_CLOCK_OVL_CFLAGS)" \
		--value "ATLAS_EXTRA=$(ATLAS_EXTRA)" \
		--value "JOURNAL_HOOK=$(JOURNAL_HOOK)" \
		--value "STJ_SAVELOAD=$(STJ_SAVELOAD)"

ATLAS_EXTRA_ARGS = $(addprefix --extra ,$(ATLAS_EXTRA))
ATLAS_BINDING_ARGS = $(foreach path,$(ATLAS_BINDING_PATHS),--binding-path "$(path)") \
	$(ATLAS_BINDING_EXCLUDES) \
	--binding-value "PORT=$(PORT)" \
	--binding-value "MQTT_HOST=$(MQTT_HOST)" \
	--binding-value "MQTT_PORT=$(MQTT_PORT)" \
	--binding-value "MQTT_CODE=$(MQTT_CODE)" \
	--binding-value "TIME_HOST=$(TIME_HOST)" \
	--binding-value "UART_BACKEND=$(UART_BACKEND)" \
	--binding-value "NET_BACKEND=$(NET_BACKEND)" \
	--binding-value "FS_BACKEND=$(FS_BACKEND)" \
	--binding-value "OVERLAY_BLOCK_SIZE=$(OVERLAY_BLOCK_SIZE)" \
	--binding-value "OVERLAY_SIZE_LIMIT=$(OVERLAY_SIZE_LIMIT)" \
	--binding-value "ZX_TARGET_CFLAGS=$(ZX_TARGET_CFLAGS)"

$(OVL_ATLAS_TABLE): tools/gen_overlay_atlas.py $(ATLAS_SEED_FILE) $(ATLAS_BINDING_DEPS) | $(BUILD_DIR)
	"$(PYTHON)" tools/gen_overlay_atlas.py --build-dir $(BUILD_DIR) --name $(ZX_NAME) --out $(BUILD_DIR)/$(ZX_NAME).OVL.bootstrap --asm-out $(OVL_ATLAS_TABLE) --bootstrap-asm --seed-json $(ATLAS_SEED_FILE) --seed-profile $(ATLAS_SEED_PROFILE) --block-size $(OVERLAY_BLOCK_SIZE) --size-limit $(OVERLAY_SIZE_LIMIT) $(addprefix --extra ,$(ATLAS_EXTRA)) $(ATLAS_BINDING_ARGS)

$(ZX_TAP): $(BUILD_CONFIG_STAMP) $(SPECTRUM_SRC) $(SPECTRUM_HEADERS) $(SPXN_RESIDENT_OBJS) $(UART_ASM) $(SHRINK_ASM) $(FRAME_CLOCK_ASM) $(CLASSIC_BOOT_ASM) $(EDIT_FIELD_ASM) $(SAN_ASM) $(SCREEN_ASM) $(SCREEN_INCLUDE_DEPS) $(UI_LAYOUT_ASM) $(EDITOR_STATE_ASM) $(RESIDENT_EXTRA_ASM) $(RESIDENT_INCLUDE_DEPS) asm/spectrum/text.asm $(OVERLAY_LOADER_ASM) tools/netchesszx_bool_copt tools/check_sdcc_iy_contract.py tools/check_tap_image.py tools/package_classic_boot.py tools/package_spectranext_tap.py $(SPXN_LOADING_SCREEN) $(BUILD_DAT) $(OVL_ATLAS_TABLE) | $(BUILD_DIR) $(RELEASE_DIR)
	rm -f $(BUILD_DIR)/$(ZX_NAME) $(BUILD_DIR)/$(ZX_NAME).tap $(BUILD_DIR)/$(ZX_NAME)_CODE.bin $(BUILD_DIR)/$(ZX_NAME).map
	rm -f $(BUILD_DIR)/NCHESSZX $(BUILD_DIR)/NCHESSZX.tap $(BUILD_DIR)/NCHESSZX_CODE.bin $(BUILD_DIR)/NCHESSZX.map
	rm -f $(RELEASE_DIR)/NCHESSZX.tap $(RELEASE_DIR)/NCHESSZX.OVL $(RELEASE_DIR)/NCHESSZX.DAT
	@{ cd $(BUILD_DIR) && $(ZCC) +zx $(ZX_CFLAGS) $(addprefix $(BUILD_DIR_UP),$(SPECTRUM_SRC)) $(SPXN_RESIDENT_LINK_OBJS) $(addprefix $(BUILD_DIR_UP),$(UART_ASM)) $(BUILD_DIR_UP)$(SHRINK_ASM) $(BUILD_DIR_UP)$(FRAME_CLOCK_ASM) $(BUILD_DIR_UP)$(EDIT_FIELD_ASM) $(BUILD_DIR_UP)$(SAN_ASM) $(BUILD_DIR_UP)$(SCREEN_ASM) $(BUILD_DIR_UP)$(EDITOR_STATE_ASM) $(addprefix $(BUILD_DIR_UP),$(RESIDENT_EXTRA_ASM)) $(BUILD_DIR_UP)asm/spectrum/text.asm $(BUILD_DIR_UP)$(OVERLAY_LOADER_ASM) -o $(ZX_NAME) -create-app; printf "%s\n" $$? > $(ZX_NAME).build.status; } 2>&1 | tee $(ZX_BUILD_LOG); \
	build_rc=$$(cat $(BUILD_DIR)/$(ZX_NAME).build.status 2>/dev/null || echo 1); \
	rm -f $(BUILD_DIR)/$(ZX_NAME).build.status; \
	if [ "$$build_rc" -ne 0 ]; then exit $$build_rc; fi
	@if [ "$(CLASSIC_BOOT)" = "1" ]; then \
		"$(PYTHON)" tools/package_classic_boot.py --map $(BUILD_DIR)/$(ZX_NAME).map --source $(CLASSIC_BOOT_ASM) --build-dir $(BUILD_DIR) --assembler "$(Z80ASM)" --tap $(BUILD_DIR)/$(ZX_NAME).tap; \
	fi
	@if [ "$(NET_BACKEND)" = "spectranext" ]; then \
		backend="$(BUILD_DIR)/$(ZX_NAME)_spxn_overlay_backend.bin"; \
		combined="$(BUILD_DIR)/$(ZX_NAME)_SPXN_CODE.bin"; \
		backend_size=$$(wc -c < "$$backend"); \
		pad=$$(( $(ZX_ORG) - 0x6e00 - backend_size )); \
		if [ "$$pad" -lt 0 ]; then echo "[ERR] Spectranext overlay backend exceeds 0x6e00..0x6fff"; exit 1; fi; \
		cp -f "$$backend" "$$combined"; \
		dd if=/dev/zero bs=1 count="$$pad" >> "$$combined" 2>/dev/null; \
		cat "$(BUILD_DIR)/$(ZX_NAME)_CODE.bin" >> "$$combined"; \
		rm -f "$(BUILD_DIR)/$(ZX_NAME).tap"; \
		"$(PYTHON)" tools/package_spectranext_tap.py --code "$$combined" --org 28160 --clearaddr 28159 --usraddr $(ZX_ORG) --screen $(SPXN_LOADING_SCREEN) -o "$(BUILD_DIR)/$(ZX_NAME).tap"; \
	fi
	@$(MAKE) sdcc-iy-contract-check
	"$(PYTHON)" tools/check_lowmem_layout.py --map $(BUILD_DIR)/$(ZX_NAME).map --target $(LOWMEM_LAYOUT)
	cp -f $(BUILD_DIR)/$(ZX_NAME).tap $(ZX_TAP)
	@if [ "$(NET_BACKEND)" = "spectranext" ]; then \
		"$(PYTHON)" tools/check_tap_image.py --map $(BUILD_DIR)/$(ZX_NAME).map --code-bin $(BUILD_DIR)/$(ZX_NAME)_CODE.bin --tap $(ZX_TAP) --org $(ZX_ORG) --load-org 0x6e00 --low-code-bin $(BUILD_DIR)/$(ZX_NAME)_spxn_overlay_backend.bin; \
	else \
		"$(PYTHON)" tools/check_tap_image.py --map $(BUILD_DIR)/$(ZX_NAME).map --code-bin $(BUILD_DIR)/$(ZX_NAME)_CODE.bin --tap $(ZX_TAP) --org $(ZX_ORG) $(if $(filter 1,$(CLASSIC_BOOT)),--boot-bin $(BUILD_DIR)/$(ZX_NAME)_CLASSIC_BOOT.bin --boot-org 0x6800 --clearaddr 0x67ff --usraddr $(ZX_ORG)); \
	fi

# -o preserves the converged atlas in the final pass but hides its timestamp.
ifeq ($(ATLAS_FINAL),1)
$(ZX_TAP): FORCE
endif

# Overlay recipes stay in tools/build_overlays.py so the Make command line
# stays under the Windows 8191-character cap. Export what the builder reads.
export SPXN_ATOMIC_OBJ SPXN_ATOMIC_SRC SPXN_RESOLVE_C SPXN_XFS_OVERLAY_ASM
export ATLAS_EXTRA_ARGS ATLAS_BINDING_ARGS ATLAS_EXTRA ATLAS_FINAL BUILD_DIR BUILD_DIR_UP CONFIG_OVL_CFLAGS EDIT_BUF_OVL ESX_COMMON_ASM ESX_FILEUI_ASM ESX_FILEUI_OBJ ESX_SAVELOAD_ASM ESX_SAVELOAD_OBJ GAME_PROTOCOL_MACH_SRC JOURNAL_HOOK MAKE MQTT_CONNECT_OVL_CFLAGS OVERLAY_BLOCK_SIZE OVERLAY_SIZE_LIMIT OVL_ATLAS_TABLE OVL_DEFS PYTHON RESIDENT_EXTRA_ASM SPXN_CLOCK_C SPXN_CLOCK_HEADERS SPXN_CLOCK_OVL_CFLAGS SPXN_DIR_ABS STJ_SAVELOAD ZCC ZX_NAME ZX_OVL ZX_OVL_CFLAGS ZX_Z80ASM

$(ZX_OVL): $(ZX_TAP) $(BUILD_CONFIG_STAMP) $(SPECTRUM_HEADERS) $(OVERLAY_SRC) tools/build_overlays.py tools/gen_overlay_defs.py tools/gen_overlay_atlas.py | $(BUILD_DIR) $(RELEASE_DIR)
	"$(PYTHON)" tools/build_overlays.py

FORCE:

clean: clean-spectrum clean-client

clean-spectrum:
ifneq ($(CLIENT_HOST_IS_WINDOWS),)
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File tools/clean-generated.ps1 -SpectrumOnly
else
	rm -rf $(BUILD_DIR) dist
	rm -rf build-next build-nex release/next release/nex release/Next $(BUILD_DIR)/next $(BUILD_DIR)/nex
	find src -name '*.c.asm' -delete
	rm -f $(RELEASE_DIR)/$(ZX_NAME).tap $(RELEASE_DIR)/$(ZX_NAME).OVL $(RELEASE_DIR)/$(ZX_NAME).DAT
	rm -f $(RELEASE_DIR)/NCHESSZX.tap $(RELEASE_DIR)/NCHESSZX.OVL $(RELEASE_DIR)/NCHESSZX.DAT
	rm -f $(RELEASE_DIR)/$(ZX_NAME)_MQTT_W.tap $(RELEASE_DIR)/$(ZX_NAME)_MQTT_W.OVL $(RELEASE_DIR)/$(ZX_NAME)_MQTT_W.DAT
	rm -f $(RELEASE_DIR)/$(ZX_NAME)_MQTT_B.tap $(RELEASE_DIR)/$(ZX_NAME)_MQTT_B.OVL $(RELEASE_DIR)/$(ZX_NAME)_MQTT_B.DAT
	rm -f $(RELEASE_DIR)/MQTTW $(RELEASE_DIR)/MQTTB $(RELEASE_DIR)/MQTTWKEY $(RELEASE_DIR)/MQTTBKEY $(RELEASE_DIR)/ZXCHNET.tap
	rm -f asm/overlay/rules/entry_rules.o asm/overlay/rules/rules_stub.o asm/overlay/board/entry_board.o asm/overlay/board/helpers.o asm/overlay/gui_log/entry_gui_log.o asm/overlay/input_edit/entry_input_edit.o asm/overlay/input_edit/setup_edit_line.o asm/overlay/edit/edit_buf.o asm/overlay/mqtt_connect/entry_mqtt_connect.o asm/overlay/mqtt_tx/entry_mqtt_tx.o asm/overlay/direct/entry_direct.o asm/overlay/menu_config/entry_menu_config.o asm/overlay/menu_logic/entry_menu_logic.o asm/overlay/fileui/entry_fileui.o asm/overlay/setup/entry_setup.o asm/overlay/saveload/entry_saveload.o asm/overlay/restore/entry_restore.o asm/overlay/about/entry_about.o asm/esxdos/esx_fileio_spectalk.o asm/esxdos/esx_fileui.o asm/esxdos/esx_saveload.o $(BUILD_DIR)/asm/overlay/rules/rules_stub.o $(BUILD_DIR)/board_apply_ovl.o $(BUILD_DIR)/gui_log_ovl.o $(BUILD_DIR)/input_edit_ovl.o $(BUILD_DIR)/mqtt_connect_ovl.o $(BUILD_DIR)/mqtt_tx_ovl.o $(BUILD_DIR)/direct_ovl.o $(BUILD_DIR)/status_ovl.o $(BUILD_DIR)/fileui_ovl.o $(BUILD_DIR)/saveload_ovl.o $(BUILD_DIR)/restore_ovl.o $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~ $(BUILD_DIR)/$(ZX_NAME)_RULES.OVL $(BUILD_DIR)/$(ZX_NAME)_BOARD.OVL $(BUILD_DIR)/$(ZX_NAME)_GUI_LOG.OVL $(BUILD_DIR)/$(ZX_NAME)_INPUT_EDIT.OVL $(BUILD_DIR)/$(ZX_NAME)_MQTT_CONNECT.OVL $(BUILD_DIR)/$(ZX_NAME)_MQTT_TX.OVL $(BUILD_DIR)/$(ZX_NAME)_DIRECT.OVL $(BUILD_DIR)/$(ZX_NAME)_MENU_CONFIG.OVL $(BUILD_DIR)/$(ZX_NAME)_MENU_LOGIC.OVL $(BUILD_DIR)/$(ZX_NAME)_FILEUI.OVL $(BUILD_DIR)/$(ZX_NAME)_SETUP.OVL $(BUILD_DIR)/$(ZX_NAME)_SAVELOAD.OVL $(BUILD_DIR)/$(ZX_NAME)_RESTORE.OVL $(BUILD_DIR)/$(ZX_NAME)_ABOUT.OVL $(BUILD_DIR)/$(ZX_NAME).OVL.tmp $(BUILD_DIR)/overlay_atlas_table.changed $(BUILD_DIR)/$(ZX_NAME).OVL.bootstrap 2>/dev/null || true
	rm -f asm/overlay/control/entry_control.o asm/overlay/config/entry_config.o asm/overlay/time_config/entry_time_config.o asm/overlay/time/entry_time.o asm/overlay/edit/edit_buf.o $(BUILD_DIR)/control_ovl.o $(BUILD_DIR)/config_ovl.o $(BUILD_DIR)/game_protocol_mach_ovl.o $(BUILD_DIR)/time_ovl.o $(BUILD_DIR)/spxudp_ovl.o $(BUILD_DIR)/spxtime_ovl.o $(BUILD_DIR)/$(ZX_NAME)_CONTROL.OVL $(BUILD_DIR)/$(ZX_NAME)_CONFIG.OVL $(BUILD_DIR)/$(ZX_NAME)_TIME_CONFIG.OVL $(BUILD_DIR)/$(ZX_NAME)_TIME.OVL
	rmdir $(RELEASE_DIR) 2>/dev/null || true
endif

clean-client:
ifneq ($(CLIENT_HOST_IS_WINDOWS),)
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File tools/clean-generated.ps1 -ClientOnly
else
	rm -rf client/build client/build_manual client/build_verify client/dist
	rm -rf $(RELEASE_DIR)/shatranj-client $(RELEASE_DIR)/shatranj $(RELEASE_DIR)/netchesszx-client
	rm -f main.obj
	rmdir $(RELEASE_DIR) 2>/dev/null || true
endif

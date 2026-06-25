CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -Isrc
MCUMAX_CFLAGS ?= -std=gnu99 -Wall -Wextra -Isrc -Ithird_party/mcu-max/src
BUILD_DIR := build
RELEASE_DIR := release
ZCC ?= zcc
Z80ASM ?= z80asm
POWERSHELL ?= powershell
PYTHON ?= python3
CMAKE ?= cmake
QMAKE ?= qmake
ZX_EXTRA_CFLAGS ?=
CLIENT_BUILD ?= auto
CLIENT_CMAKE_BUILD_DIR ?= $(BUILD_DIR)/qt-client
CLIENT_QMAKE_BUILD_DIR ?= $(BUILD_DIR)/qmake-client
CLIENT_CMAKE_CONFIG ?= Release
CLIENT_CMAKE_ARGS ?=
CLIENT_QMAKE_ARGS ?=
CLIENT_QMAKE_PROJECT ?= client/shatranj.pro
CLIENT_MAC_APPLICATIONS_DIR ?= /Applications
HOST_UNAME := $(shell uname -s 2>/dev/null || echo unknown)
CLIENT_HOST_IS_WINDOWS := $(if $(filter Windows_NT,$(OS)),1,$(if $(COMSPEC),1,$(if $(ComSpec),1,$(if $(filter MINGW% MSYS% CYGWIN%,$(HOST_UNAME)),1,))))

PORT ?= 5000
MQTT_HOST ?= broker.hivemq.com
MQTT_PORT ?= 1883
MQTT_CODE ?= DEVROOM
APP_VERSION := $(strip $(shell cat VERSION))
ZX_NAME := SHATRANJ
ZX_ORG := 28672
ASSET_ASM := assets/spectrum/ui_runtime_assets.asm
ABOUT_BOARD := assets/spectrum/about_board.bin
SPECTRUM_SRC := src/spectrum/app/app.c \
                src/spectrum/app/net_runtime.c \
                src/spectrum/config/session.c \
                src/common/chess/move_coords.c \
                src/common/protocol/game_protocol.c \
                src/common/protocol/game_protocol_extra.c \
                src/common/protocol/direct_session_protocol.c \
                src/common/protocol/mqtt_session_protocol.c \
                src/spectrum/session/connection.c \
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
                src/spectrum/transport/mqtt_min.c \
                src/spectrum/board/board.c \
                src/spectrum/board/san.c \
                src/spectrum/ui/gui.c \
                src/spectrum/overlay/overlay.c
PIECE_ASM := assets/spectrum/chess_pieces_16x16.asm

RULES_TEST := $(BUILD_DIR)/netchesszx_rules_test.exe
RULES_TEST_SRC := src/common/chess/position.c src/common/chess/legal.c \
                  third_party/mcu-max/src/mcu-max.c tests/rules/test_fen.c
MOVE_COORDS_TEST := $(BUILD_DIR)/netchesszx_move_coords_test.exe
MOVE_COORDS_TEST_SRC := src/common/chess/move_coords.c tests/common/test_move_coords.c
BOARD_TEST := $(BUILD_DIR)/netchesszx_spectrum_board_test.exe
BOARD_TEST_SRC := src/common/chess/move_coords.c \
                  src/spectrum/board/board.c src/spectrum/board/san.c \
                  src/spectrum/board/rules_compact.c \
                  src/common/chess/legal.c third_party/mcu-max/src/mcu-max.c \
                  tests/spectrum/test_board.c
MQTT_TEST := $(BUILD_DIR)/netchesszx_mqtt_test.exe
MQTT_TEST_SRC := src/common/mqtt/mqtt.c src/spectrum/transport/mqtt_min.c \
                 tests/net/test_mqtt.c
PROTOCOL_TEST := $(BUILD_DIR)/netchesszx_protocol_test.exe
PROTOCOL_TEST_SRC := src/common/protocol/messages.c tests/net/test_protocol.c
GAME_PROTOCOL_TEST := $(BUILD_DIR)/netchesszx_game_protocol_test.exe
GAME_PROTOCOL_TEST_SRC := src/common/protocol/game_protocol.c \
                           src/common/protocol/game_protocol_extra.c \
                           src/common/protocol/game_protocol_format.c \
                           tests/net/test_game_protocol.c
ESP_AT_TEST := $(BUILD_DIR)/netchesszx_esp_at_test.exe
ESP_AT_TEST_SRC := src/spectrum/transport/esp_at.c src/common/protocol/game_protocol.c tests/net/test_esp_at.c
MQTT_SESSION_PROTOCOL_TEST := $(BUILD_DIR)/netchesszx_mqtt_session_protocol_test.exe
MQTT_SESSION_PROTOCOL_TEST_SRC := src/common/protocol/mqtt_session_protocol.c \
                                  src/common/protocol/mqtt_session_protocol_format.c \
                                  tests/net/test_mqtt_session_protocol.c
DIRECT_SESSION_PROTOCOL_TEST := $(BUILD_DIR)/netchesszx_direct_session_protocol_test.exe
DIRECT_SESSION_PROTOCOL_TEST_SRC := src/common/protocol/direct_session_protocol.c \
                                    src/common/protocol/game_protocol.c \
                                    tests/net/test_direct_session_protocol.c
KEEPALIVE_PROTOCOL_TEST := $(BUILD_DIR)/netchesszx_keepalive_protocol_test.exe
KEEPALIVE_PROTOCOL_TEST_SRC := src/spectrum/transport/keepalive_protocol.c \
                               src/common/protocol/game_protocol.c \
                               tests/net/test_keepalive_protocol.c
SESSION_CONFIG_TEST := $(BUILD_DIR)/netchesszx_session_config_test.exe
SESSION_CONFIG_TEST_SRC := src/spectrum/config/session.c tests/spectrum/test_session_config.c
SESSION_PING_TEST := $(BUILD_DIR)/netchesszx_session_ping_test.exe
SESSION_PING_TEST_SRC := src/spectrum/session/ping.c tests/spectrum/test_session_ping.c
SESSION_POLL_TEST := $(BUILD_DIR)/netchesszx_session_poll_test.exe
SESSION_POLL_TEST_SRC := src/spectrum/config/session.c \
                          src/common/protocol/direct_session_protocol.c \
                          src/common/protocol/game_protocol.c \
                          src/common/protocol/game_protocol_extra.c \
                          src/common/protocol/mqtt_session_protocol.c \
                         src/spectrum/transport/keepalive_protocol.c \
                         src/spectrum/session/mqtt.c src/spectrum/session/event.c \
                         src/spectrum/session/ping.c src/spectrum/session/poll.c \
                         tests/spectrum/test_session_poll.c
SESSION_DIRECT_TEST := $(BUILD_DIR)/netchesszx_session_direct_test.exe
SESSION_DIRECT_TEST_SRC := src/spectrum/config/session.c \
                            src/common/protocol/direct_session_protocol.c \
                            src/common/protocol/game_protocol.c \
                            src/spectrum/session/direct.c \
                            tests/spectrum/test_session_direct.c
SESSION_EVENT_TEST := $(BUILD_DIR)/netchesszx_session_event_test.exe
SESSION_EVENT_TEST_SRC := src/spectrum/config/session.c \
                           src/common/protocol/direct_session_protocol.c \
                           src/common/protocol/game_protocol.c \
                           src/common/protocol/game_protocol_extra.c \
                           src/common/protocol/mqtt_session_protocol.c \
                          src/spectrum/transport/keepalive_protocol.c \
                          src/spectrum/session/mqtt.c src/spectrum/session/event.c \
                          tests/spectrum/test_session_event.c
SESSION_MQTT_TEST := $(BUILD_DIR)/netchesszx_session_mqtt_test.exe
SESSION_MQTT_TEST_SRC := src/spectrum/config/session.c \
                         src/common/protocol/mqtt_session_protocol.c \
                         src/spectrum/session/mqtt.c \
                         tests/spectrum/test_session_mqtt.c
SESSION_OUTGOING_TEST := $(BUILD_DIR)/netchesszx_session_outgoing_test.exe
SESSION_OUTGOING_TEST_SRC := src/spectrum/config/session.c src/common/protocol/messages.c \
                             src/spectrum/session/outgoing.c \
                             tests/spectrum/test_session_outgoing.c
ZX_TAP := $(RELEASE_DIR)/$(ZX_NAME).tap
ZX_OVL := $(RELEASE_DIR)/$(ZX_NAME).OVL
ZX_DAT := $(RELEASE_DIR)/$(ZX_NAME).DAT
BUILD_DAT := $(BUILD_DIR)/$(ZX_NAME).DAT
OVL_DEFS := $(BUILD_DIR)/overlay_defs.asm
VERSION_ASM := $(BUILD_DIR)/version_banner.asm
ABI_MANIFEST := $(BUILD_DIR)/abi_manifest.json
ABI_BASELINE := docs/abi_manifest.baseline.json
SIZE_REPORT := $(BUILD_DIR)/size_report.json
SIZE_BASELINE := docs/size_report.baseline.json
LITERAL_REPORT := $(BUILD_DIR)/literal_report.json
ZX_BUILD_LOG := $(BUILD_DIR)/$(ZX_NAME).build.log

UART_ASM := asm/uart/divmmc_uart.asm

ZX_CLIB := sdcc_iy
ZX_ASMFLAGS := -DNETCHESSZX_SDCC_IY
ZX_SDCC_CFLAGS := -compiler=sdcc --opt-code-size --fomit-frame-pointer \
                  -DNETCHESSZX_SDCC_IY \
                  -Ca$(ZX_ASMFLAGS)
ZX_LDFLAGS := -Wl,--gc-sections
ZX_Z80ASM := $(Z80ASM) $(ZX_ASMFLAGS)

ZX_CFLAGS := -vn -startup=31 -clib=$(ZX_CLIB) -SO3 -m \
             $(ZX_SDCC_CFLAGS) \
             -I../src \
             -pragma-define:CLIB_MALLOC_HEAP_SIZE=0 \
             -pragma-define:CLIB_STDIO_HEAP_SIZE=0 \
             -pragma-define:CRT_ENABLE_STDIO=0 \
             -pragma-define:CRT_STACK_SIZE=512 \
              -DNETCHESSZX_FIXED_LOW_RAM \
             -zorg=$(ZX_ORG) \
             -DNETCHESSZX_PORT=$(PORT) \
             -DNETCHESSZX_MQTT_HOST_TOKEN=$(MQTT_HOST) \
             -DNETCHESSZX_MQTT_PORT=$(MQTT_PORT) \
             -DNETCHESSZX_MQTT_CODE_TOKEN=$(MQTT_CODE) \
             $(ZX_LDFLAGS) \
             $(ZX_EXTRA_CFLAGS)
ZX_OVL_CFLAGS := -vn -clib=$(ZX_CLIB) -SO3 -m \
                 $(ZX_SDCC_CFLAGS) \
                 -I../src \
               -DNETCHESSZX_FIXED_LOW_RAM \
                 $(ZX_EXTRA_CFLAGS)
MQTT_CONNECT_OVL_CFLAGS := -DNETCHESSZX_MQTT_PORT=$(MQTT_PORT)

.NOTPARALLEL:

.PHONY: all check module-guards layering-check layering-report overlay-cap-check overlay-cap-report overlay-entry-abi-check test tap tap-direct-overlay sdcc-iy-contract-check abi-manifest abi-baseline abi-check size-report size-baseline size-check literal-report client qt-client pc-client mac-client client-msvc client-cmake client-qmake clean clean-spectrum clean-client FORCE

all: check clean test tap client

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(RULES_TEST): $(RULES_TEST_SRC) src/common/chess/position.h src/common/chess/legal.h | $(BUILD_DIR)
	$(CC) $(MCUMAX_CFLAGS) $(RULES_TEST_SRC) -o $@

$(MOVE_COORDS_TEST): $(MOVE_COORDS_TEST_SRC) src/common/chess/move_coords.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(MOVE_COORDS_TEST_SRC) -o $@

$(BOARD_TEST): $(BOARD_TEST_SRC) src/spectrum/board/board.h src/spectrum/board/san.h src/spectrum/board/rules_compact.h | $(BUILD_DIR)
	$(CC) $(MCUMAX_CFLAGS) -DNETCHESSZX_HOST_TEST $(BOARD_TEST_SRC) -o $@

$(MQTT_TEST): $(MQTT_TEST_SRC) src/common/mqtt/mqtt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(MQTT_TEST_SRC) -o $@

$(PROTOCOL_TEST): $(PROTOCOL_TEST_SRC) src/common/protocol/messages.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(PROTOCOL_TEST_SRC) -o $@

$(GAME_PROTOCOL_TEST): $(GAME_PROTOCOL_TEST_SRC) src/common/protocol/game_protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GAME_PROTOCOL_TEST_SRC) -o $@

$(ESP_AT_TEST): $(ESP_AT_TEST_SRC) src/spectrum/transport/esp_at.h src/spectrum/platform/net_runtime.h src/spectrum/platform/uart.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DNETCHESSZX_HOST_TEST $(ESP_AT_TEST_SRC) -o $@

$(MQTT_SESSION_PROTOCOL_TEST): $(MQTT_SESSION_PROTOCOL_TEST_SRC) src/common/protocol/mqtt_session_protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(MQTT_SESSION_PROTOCOL_TEST_SRC) -o $@

$(DIRECT_SESSION_PROTOCOL_TEST): $(DIRECT_SESSION_PROTOCOL_TEST_SRC) src/common/protocol/direct_session_protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DIRECT_SESSION_PROTOCOL_TEST_SRC) -o $@

$(KEEPALIVE_PROTOCOL_TEST): $(KEEPALIVE_PROTOCOL_TEST_SRC) src/spectrum/transport/keepalive_protocol.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(KEEPALIVE_PROTOCOL_TEST_SRC) -o $@

$(SESSION_CONFIG_TEST): $(SESSION_CONFIG_TEST_SRC) src/spectrum/config/session.h src/spectrum/transport/net.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_CONFIG_TEST_SRC) -o $@

$(SESSION_PING_TEST): $(SESSION_PING_TEST_SRC) src/spectrum/session/ping.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_PING_TEST_SRC) -o $@

$(SESSION_POLL_TEST): $(SESSION_POLL_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/poll.h src/spectrum/session/ping.h src/spectrum/session/event.h src/spectrum/transport/link.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_POLL_TEST_SRC) -o $@

$(SESSION_DIRECT_TEST): $(SESSION_DIRECT_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/direct.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_DIRECT_TEST_SRC) -o $@

$(SESSION_EVENT_TEST): $(SESSION_EVENT_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/event.h src/spectrum/session/mqtt.h src/common/protocol/messages.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_EVENT_TEST_SRC) -o $@

$(SESSION_MQTT_TEST): $(SESSION_MQTT_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/mqtt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_MQTT_TEST_SRC) -o $@

$(SESSION_OUTGOING_TEST): $(SESSION_OUTGOING_TEST_SRC) src/spectrum/config/session.h src/spectrum/session/outgoing.h src/common/protocol/messages.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SESSION_OUTGOING_TEST_SRC) -o $@

module-guards: layering-check overlay-cap-check overlay-entry-abi-check

layering-check: tools/check_layering.py docs/layering_allowlist.json docs/overlay_state_allowlist.json
	$(PYTHON) tools/check_layering.py --root . --allowlist docs/layering_allowlist.json --overlay-state-allowlist docs/overlay_state_allowlist.json

layering-report: tools/check_layering.py docs/layering_allowlist.json docs/overlay_state_allowlist.json
	$(PYTHON) tools/check_layering.py --root . --allowlist docs/layering_allowlist.json --overlay-state-allowlist docs/overlay_state_allowlist.json --report

overlay-cap-check: tools/check_overlay_caps.py docs/overlay_capabilities.json tools/gen_overlay_defs.py
	$(PYTHON) tools/check_overlay_caps.py --root . --policy docs/overlay_capabilities.json

overlay-cap-report: tools/check_overlay_caps.py docs/overlay_capabilities.json tools/gen_overlay_defs.py
	$(PYTHON) tools/check_overlay_caps.py --root . --policy docs/overlay_capabilities.json --report

overlay-entry-abi-check: tools/check_overlay_entry_abi.py
	$(PYTHON) tools/check_overlay_entry_abi.py --root .

check: module-guards | $(BUILD_DIR) $(RELEASE_DIR)
	@fail=0; \
	for t in $(ZCC) $(Z80ASM) z88dk-appmake grep sed head wc dd $(PYTHON); do \
		command -v "$$t" >/dev/null 2>&1 || { echo "[ERR] Missing tool: $$t"; fail=1; }; \
	done; \
	for f in $(SPECTRUM_SRC) $(UART_ASM) asm/spectrum/screen.asm asm/spectrum/text.asm asm/esxdos/overlay_loader.asm $(PIECE_ASM) tools/gen_overlay_defs.py; do \
		test -f "$$f" || { echo "[ERR] Missing source: $$f"; fail=1; }; \
	done; \
	exit $$fail

test: $(RULES_TEST) $(MOVE_COORDS_TEST) $(BOARD_TEST) $(MQTT_TEST) $(PROTOCOL_TEST) $(GAME_PROTOCOL_TEST) $(MQTT_SESSION_PROTOCOL_TEST) $(DIRECT_SESSION_PROTOCOL_TEST) $(KEEPALIVE_PROTOCOL_TEST) $(ESP_AT_TEST) $(SESSION_CONFIG_TEST) $(SESSION_PING_TEST) $(SESSION_POLL_TEST) $(SESSION_DIRECT_TEST) $(SESSION_EVENT_TEST) $(SESSION_MQTT_TEST) $(SESSION_OUTGOING_TEST)
	./$(RULES_TEST)
	./$(MOVE_COORDS_TEST)
	./$(BOARD_TEST)
	./$(MQTT_TEST)
	./$(PROTOCOL_TEST)
	./$(ESP_AT_TEST)
	./$(GAME_PROTOCOL_TEST)
	./$(MQTT_SESSION_PROTOCOL_TEST)
	./$(DIRECT_SESSION_PROTOCOL_TEST)
	./$(KEEPALIVE_PROTOCOL_TEST)
	./$(SESSION_CONFIG_TEST)
	./$(SESSION_PING_TEST)
	./$(SESSION_POLL_TEST)
	./$(SESSION_DIRECT_TEST)
	./$(SESSION_EVENT_TEST)
	./$(SESSION_MQTT_TEST)
	./$(SESSION_OUTGOING_TEST)

tap: $(ZX_TAP)
tap: $(ZX_OVL)
tap: $(ZX_DAT)

tap-direct-overlay:
	$(MAKE) tap

sdcc-iy-contract-check: tools/check_sdcc_iy_contract.py | $(BUILD_DIR)
	$(PYTHON) tools/check_sdcc_iy_contract.py --zcc "$(ZCC)" --build-dir "$(BUILD_DIR)"

abi-manifest: tap tools/gen_abi_manifest.py tools/gen_overlay_defs.py src/spectrum/overlay/overlay_api.h src/spectrum/overlay/overlay.h | $(BUILD_DIR)
	$(PYTHON) tools/gen_abi_manifest.py $(BUILD_DIR)/$(ZX_NAME).map --output $(ABI_MANIFEST)

abi-baseline: tap tools/gen_abi_manifest.py tools/gen_overlay_defs.py src/spectrum/overlay/overlay_api.h src/spectrum/overlay/overlay.h | $(BUILD_DIR)
	$(PYTHON) tools/gen_abi_manifest.py $(BUILD_DIR)/$(ZX_NAME).map --output $(ABI_MANIFEST) --write-baseline $(ABI_BASELINE)

abi-check: tap tools/gen_abi_manifest.py tools/gen_overlay_defs.py src/spectrum/overlay/overlay_api.h src/spectrum/overlay/overlay.h | $(BUILD_DIR)
	$(PYTHON) tools/gen_abi_manifest.py $(BUILD_DIR)/$(ZX_NAME).map --output $(ABI_MANIFEST) --baseline $(ABI_BASELINE) --fail-on-missing-baseline $(ABI_CHECK_FLAGS)

size-report: tap tools/gen_size_report.py | $(BUILD_DIR)
	$(PYTHON) tools/gen_size_report.py --map $(BUILD_DIR)/$(ZX_NAME).map --code-bin $(BUILD_DIR)/$(ZX_NAME)_CODE.bin --tap $(ZX_TAP) --ovl $(ZX_OVL) --dat $(ZX_DAT) --overlay-sizes $(BUILD_DIR)/overlay_sizes.json --output $(SIZE_REPORT)

size-baseline: tap tools/gen_size_report.py | $(BUILD_DIR)
	$(PYTHON) tools/gen_size_report.py --map $(BUILD_DIR)/$(ZX_NAME).map --code-bin $(BUILD_DIR)/$(ZX_NAME)_CODE.bin --tap $(ZX_TAP) --ovl $(ZX_OVL) --dat $(ZX_DAT) --overlay-sizes $(BUILD_DIR)/overlay_sizes.json --output $(SIZE_REPORT) --write-baseline $(SIZE_BASELINE)

size-check: tap tools/gen_size_report.py | $(BUILD_DIR)
	$(PYTHON) tools/gen_size_report.py --map $(BUILD_DIR)/$(ZX_NAME).map --code-bin $(BUILD_DIR)/$(ZX_NAME)_CODE.bin --tap $(ZX_TAP) --ovl $(ZX_OVL) --dat $(ZX_DAT) --overlay-sizes $(BUILD_DIR)/overlay_sizes.json --output $(SIZE_REPORT) --baseline $(SIZE_BASELINE) --fail-on-missing-baseline $(SIZE_CHECK_FLAGS)

literal-report: tools/gen_literal_report.py | $(BUILD_DIR)
	$(PYTHON) tools/gen_literal_report.py --root . --output $(LITERAL_REPORT)

client:
ifeq ($(CLIENT_BUILD),auto)
ifneq ($(CLIENT_HOST_IS_WINDOWS),)
	@$(MAKE) client-msvc
else
	@if command -v "$(CMAKE)" >/dev/null 2>&1; then \
		$(MAKE) client-cmake; \
	elif command -v "$(QMAKE)" >/dev/null 2>&1; then \
		$(MAKE) client-qmake; \
	else \
		echo "[ERR] Missing Qt build tool: install cmake or qmake, or run make CLIENT_BUILD=msvc client on Windows"; \
		exit 1; \
	fi
endif
else
	@$(MAKE) client-$(CLIENT_BUILD)
endif

qt-client: client
pc-client: client
mac-client: client

client-msvc:
	@command -v "$(POWERSHELL)" >/dev/null 2>&1 || { echo "[ERR] Missing PowerShell: set POWERSHELL=pwsh or use CLIENT_BUILD=cmake/qmake"; exit 1; }
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File client/build-msvc.ps1

client-cmake:
	@command -v "$(CMAKE)" >/dev/null 2>&1 || { echo "[ERR] Missing cmake: set CMAKE=/path/to/cmake or use CLIENT_BUILD=qmake"; exit 1; }
	$(CMAKE) -S client -B "$(CLIENT_CMAKE_BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CLIENT_CMAKE_CONFIG)" -DNETCHESSZX_MAC_APPLICATIONS_DIR="$(CLIENT_MAC_APPLICATIONS_DIR)" $(CLIENT_CMAKE_ARGS)
	$(CMAKE) --build "$(CLIENT_CMAKE_BUILD_DIR)" --config "$(CLIENT_CMAKE_CONFIG)"

client-qmake: | $(BUILD_DIR)
	@command -v "$(QMAKE)" >/dev/null 2>&1 || { echo "[ERR] Missing qmake: set QMAKE=/path/to/qmake or use CLIENT_BUILD=cmake"; exit 1; }
	mkdir -p "$(CLIENT_QMAKE_BUILD_DIR)"
	cd "$(CLIENT_QMAKE_BUILD_DIR)" && "$(QMAKE)" "$(abspath $(CLIENT_QMAKE_PROJECT))" CONFIG+=release NETCHESSZX_MAC_APPLICATIONS_DIR="$(CLIENT_MAC_APPLICATIONS_DIR)" $(CLIENT_QMAKE_ARGS)
	$(MAKE) -C "$(CLIENT_QMAKE_BUILD_DIR)"

$(RELEASE_DIR):
	mkdir -p $(RELEASE_DIR)

$(VERSION_ASM): VERSION | $(BUILD_DIR)
	printf '%s\n' 'SECTION code_user' 'PUBLIC _netchesszx_version_banner_msg' '_netchesszx_version_banner_msg:' '    DEFM "version $(APP_VERSION)",0' > $@

$(BUILD_DAT): tools/gen_assets.py $(ASSET_ASM) $(PIECE_ASM) $(ABOUT_BOARD) asm/spectrum/screen.asm asm/esxdos/overlay_loader.asm | $(BUILD_DIR)
	$(PYTHON) tools/gen_assets.py $(ASSET_ASM) $(PIECE_ASM) asm/spectrum/screen.asm asm/esxdos/overlay_loader.asm $(ABOUT_BOARD) $(BUILD_DAT)

$(ZX_DAT): $(BUILD_DAT) | $(RELEASE_DIR)
	cp -f $(BUILD_DAT) $(ZX_DAT)

$(ZX_TAP): FORCE $(SPECTRUM_SRC) $(UART_ASM) asm/spectrum/screen.asm asm/spectrum/text.asm asm/esxdos/overlay_loader.asm tools/check_sdcc_iy_contract.py $(BUILD_DAT) $(VERSION_ASM) | $(BUILD_DIR) $(RELEASE_DIR)
	rm -f $(BUILD_DIR)/$(ZX_NAME) $(BUILD_DIR)/$(ZX_NAME).tap $(BUILD_DIR)/$(ZX_NAME)_CODE.bin $(BUILD_DIR)/$(ZX_NAME).map $(BUILD_DIR)/$(ZX_NAME)_TRIMMED.tap
	rm -f $(BUILD_DIR)/NCHESSZX $(BUILD_DIR)/NCHESSZX.tap $(BUILD_DIR)/NCHESSZX_CODE.bin $(BUILD_DIR)/NCHESSZX.map
	rm -f $(RELEASE_DIR)/NCHESSZX.tap $(RELEASE_DIR)/NCHESSZX.OVL $(RELEASE_DIR)/NCHESSZX.DAT
	@{ cd $(BUILD_DIR) && $(ZCC) +zx $(ZX_CFLAGS) $(addprefix ../,$(SPECTRUM_SRC)) ../$(UART_ASM) ../asm/spectrum/screen.asm ../asm/spectrum/text.asm ../asm/esxdos/overlay_loader.asm version_banner.asm -o $(ZX_NAME) -create-app; printf "%s\n" $$? > $(ZX_NAME).build.status; } 2>&1 | tee $(ZX_BUILD_LOG); \
	build_rc=$$(cat $(BUILD_DIR)/$(ZX_NAME).build.status 2>/dev/null || echo 1); \
	rm -f $(BUILD_DIR)/$(ZX_NAME).build.status; \
	if [ "$$build_rc" -ne 0 ]; then exit $$build_rc; fi
	@$(MAKE) sdcc-iy-contract-check
	@BSS=$$(grep '__BSS_END_tail ' $(BUILD_DIR)/$(ZX_NAME).map | sed -n 's/.*= \$$\([0-9A-Fa-f]*\).*/\1/p' | head -1); \
	if [ -z "$$BSS" ]; then \
		BSS=$$(grep -E '(__BSS_END |__bss_end )' $(BUILD_DIR)/$(ZX_NAME).map | sed -n 's/.*= \$$\([0-9A-Fa-f]*\).*/\1/p' | head -1); \
	fi; \
	if [ -z "$$BSS" ]; then \
		printf "[ERR] BSS_END symbol not found in $(BUILD_DIR)/$(ZX_NAME).map\n"; \
		exit 1; \
	fi; \
	bss_dec=$$((0x$$BSS)); limit_dec=$$((0xFE00)); min_gap_dec=1024; free=$$((limit_dec - bss_dec)); \
	if [ "$$free" -lt "$$min_gap_dec" ]; then \
		printf "[ERR] BSS_END 0x$$BSS leaves $$free bytes before stack guard, below $$min_gap_dec byte floor\n"; \
		exit 1; \
	fi; \
	printf "[OK] BSS_END 0x$$BSS leaves $$free bytes before stack guard (floor $$min_gap_dec)\n"
	@DATA_TAIL=$$(grep '__data_compiler_tail ' $(BUILD_DIR)/$(ZX_NAME).map | sed -n 's/.*= \$$\([0-9A-Fa-f]*\).*/\1/p' | head -1); \
	BIN=$(BUILD_DIR)/$(ZX_NAME)_CODE.bin; \
	if [ -z "$$DATA_TAIL" ] || [ ! -f "$$BIN" ]; then \
		printf "[WARN] BSS trim skipped\n"; \
		cp -f $(BUILD_DIR)/$(ZX_NAME).tap $(ZX_TAP) || exit 1; \
	else \
		trim=$$((0x$$DATA_TAIL - $(ZX_ORG))); \
		full=$$(wc -c < "$$BIN"); \
		if [ "$$trim" -le 0 ] || [ "$$trim" -gt "$$full" ]; then \
			printf "[WARN] BSS trim skipped: invalid trim size $$trim for $$full bytes\n"; \
			cp -f $(BUILD_DIR)/$(ZX_NAME).tap $(ZX_TAP) || exit 1; \
		else \
			head -c "$$trim" "$$BIN" > $(BUILD_DIR)/$(ZX_NAME)_TRIMMED.bin; \
			z88dk-appmake +zx -b $(BUILD_DIR)/$(ZX_NAME)_TRIMMED.bin --org $(ZX_ORG) --blockname $(ZX_NAME) --usraddr $(ZX_ORG) -o $(BUILD_DIR)/$(ZX_NAME)_TRIMMED.tap; \
			appmake_rc=$$?; \
			rm -f $(BUILD_DIR)/$(ZX_NAME)_TRIMMED.bin; \
			if [ "$$appmake_rc" -ne 0 ] || [ ! -s $(BUILD_DIR)/$(ZX_NAME)_TRIMMED.tap ]; then \
				printf "[ERR] TAP generation failed: $(BUILD_DIR)/$(ZX_NAME)_TRIMMED.tap\n"; \
				exit 1; \
			fi; \
			cp -f $(BUILD_DIR)/$(ZX_NAME)_TRIMMED.tap $(ZX_TAP) || exit 1; \
			rm -f $(BUILD_DIR)/$(ZX_NAME)_TRIMMED.tap; \
			saved=$$((full - trim)); \
			printf "[OK] BSS trimmed: $$full -> $$trim bytes (-$$saved bytes)\n"; \
		fi; \
	fi; \
	if [ ! -s $(ZX_TAP) ]; then \
		printf "[ERR] TAP missing after build: $(ZX_TAP)\n"; \
		exit 1; \
	fi

$(ZX_OVL): $(ZX_TAP) asm/overlay/rules/entry_rules.asm asm/overlay/rules/rules_stub.asm asm/overlay/hints/entry_hints.asm asm/overlay/board/entry_board.asm asm/overlay/board/helpers.asm src/spectrum/overlay/board_apply_ovl.c asm/overlay/gui_log/entry_gui_log.asm src/spectrum/overlay/gui_log_ovl.c asm/overlay/mqtt_connect/entry_mqtt_connect.asm src/spectrum/overlay/mqtt_connect_ovl.c asm/overlay/mqtt_tx/entry_mqtt_tx.asm src/spectrum/overlay/mqtt_tx_ovl.c asm/overlay/direct/entry_direct.asm src/spectrum/overlay/direct_ovl.c asm/overlay/menu_config/entry_menu_config.asm src/spectrum/overlay/menu_config_ovl.c asm/overlay/menu_logic/entry_menu_logic.asm src/spectrum/overlay/menu_logic_ovl.c asm/overlay/status/entry_status.asm src/spectrum/overlay/status_ovl.c tools/gen_overlay_defs.py src/spectrum/overlay/overlay_api.h | $(BUILD_DIR) $(RELEASE_DIR)
	@SLOT=$$(grep '_overlay_code_slot ' $(BUILD_DIR)/$(ZX_NAME).map | sed -n 's/.*= \$$\([0-9A-Fa-f]*\).*/\1/p' | head -1); \
	if [ -z "$$SLOT" ]; then \
		printf "[ERR] _overlay_code_slot not found in $(BUILD_DIR)/$(ZX_NAME).map\n"; \
		exit 1; \
	fi; \
	echo "  overlay_code_slot = 0x$$SLOT"; \
	$(PYTHON) tools/gen_overlay_defs.py $(BUILD_DIR)/$(ZX_NAME).map > $(OVL_DEFS) || exit 1; \
	echo "  overlay_defs.asm generated"; \
	rm -f $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~; \
	$(ZX_Z80ASM) asm/overlay/rules/entry_rules.asm 2>&1 || exit 1; \
	$(ZX_Z80ASM) asm/overlay/rules/rules_stub.asm 2>&1 || exit 1; \
	$(ZX_Z80ASM) -b -r0x$$SLOT -o=$(BUILD_DIR)/$(ZX_NAME)_RULES.OVL \
		asm/overlay/rules/entry_rules.o asm/overlay/rules/rules_stub.o 2>&1 || exit 1; \
	ovl_size=$$(wc -c < $(BUILD_DIR)/$(ZX_NAME)_RULES.OVL); \
	if [ "$$ovl_size" -gt 2048 ]; then \
		printf "[ERR] $(ZX_NAME)_RULES.OVL too large: $$ovl_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	$(ZX_Z80ASM) asm/overlay/hints/entry_hints.asm 2>&1 || exit 1; \
	$(ZX_Z80ASM) -D=HINTS_OVL -O=$(BUILD_DIR) asm/overlay/rules/rules_stub.asm 2>&1 || exit 1; \
	rm -f $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~; \
	$(ZX_Z80ASM) -b -r0x$$SLOT -o=$(BUILD_DIR)/$(ZX_NAME)_HINTS.OVL \
		asm/overlay/hints/entry_hints.o $(BUILD_DIR)/asm/overlay/rules/rules_stub.o $(OVL_DEFS) 2>&1 || exit 1; \
	hints_size=$$(wc -c < $(BUILD_DIR)/$(ZX_NAME)_HINTS.OVL); \
	if [ "$$hints_size" -gt 2048 ]; then \
		printf "[ERR] $(ZX_NAME)_HINTS.OVL too large: $$hints_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	$(ZX_Z80ASM) asm/overlay/board/entry_board.asm 2>&1 || exit 1; \
	$(ZX_Z80ASM) asm/overlay/board/helpers.asm 2>&1 || exit 1; \
	(cd $(BUILD_DIR) && $(ZCC) +z80 $(ZX_OVL_CFLAGS) -c ../src/spectrum/overlay/board_apply_ovl.c -o board_apply_ovl.o) 2>&1 || exit 1; \
	rm -f $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~; \
	$(ZX_Z80ASM) -b -r0x$$SLOT -o=$(BUILD_DIR)/$(ZX_NAME)_BOARD.OVL \
		asm/overlay/board/entry_board.o asm/overlay/board/helpers.o $(BUILD_DIR)/board_apply_ovl.o $(OVL_DEFS) 2>&1 || exit 1; \
	board_size=$$(wc -c < $(BUILD_DIR)/$(ZX_NAME)_BOARD.OVL); \
	if [ "$$board_size" -gt 2048 ]; then \
		printf "[ERR] $(ZX_NAME)_BOARD.OVL too large: $$board_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	$(ZX_Z80ASM) asm/overlay/gui_log/entry_gui_log.asm 2>&1 || exit 1; \
	(cd $(BUILD_DIR) && $(ZCC) +z80 $(ZX_OVL_CFLAGS) -c ../src/spectrum/overlay/gui_log_ovl.c -o gui_log_ovl.o) 2>&1 || exit 1; \
	rm -f $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~; \
	$(ZX_Z80ASM) -b -r0x$$SLOT -o=$(BUILD_DIR)/$(ZX_NAME)_GUI_LOG.OVL \
		asm/overlay/gui_log/entry_gui_log.o $(BUILD_DIR)/gui_log_ovl.o $(OVL_DEFS) 2>&1 || exit 1; \
	gui_log_size=$$(wc -c < $(BUILD_DIR)/$(ZX_NAME)_GUI_LOG.OVL); \
	if [ "$$gui_log_size" -gt 2048 ]; then \
		printf "[ERR] $(ZX_NAME)_GUI_LOG.OVL too large: $$gui_log_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	$(ZX_Z80ASM) asm/overlay/mqtt_connect/entry_mqtt_connect.asm 2>&1 || exit 1; \
	(cd $(BUILD_DIR) && $(ZCC) +z80 $(ZX_OVL_CFLAGS) $(MQTT_CONNECT_OVL_CFLAGS) -c ../src/spectrum/overlay/mqtt_connect_ovl.c -o mqtt_connect_ovl.o) 2>&1 || exit 1; \
	rm -f $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~; \
	$(ZX_Z80ASM) -b -r0x$$SLOT -o=$(BUILD_DIR)/$(ZX_NAME)_MQTT_CONNECT.OVL \
		asm/overlay/mqtt_connect/entry_mqtt_connect.o $(BUILD_DIR)/mqtt_connect_ovl.o $(OVL_DEFS) 2>&1 || exit 1; \
	mqtt_connect_size=$$(wc -c < $(BUILD_DIR)/$(ZX_NAME)_MQTT_CONNECT.OVL); \
	if [ "$$mqtt_connect_size" -gt 2048 ]; then \
		printf "[ERR] $(ZX_NAME)_MQTT_CONNECT.OVL too large: $$mqtt_connect_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	$(ZX_Z80ASM) asm/overlay/mqtt_tx/entry_mqtt_tx.asm 2>&1 || exit 1; \
	(cd $(BUILD_DIR) && $(ZCC) +z80 $(ZX_OVL_CFLAGS) -c ../src/spectrum/overlay/mqtt_tx_ovl.c -o mqtt_tx_ovl.o) 2>&1 || exit 1; \
	rm -f $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~; \
	$(ZX_Z80ASM) -b -r0x$$SLOT -o=$(BUILD_DIR)/$(ZX_NAME)_MQTT_TX.OVL \
		asm/overlay/mqtt_tx/entry_mqtt_tx.o $(BUILD_DIR)/mqtt_tx_ovl.o $(OVL_DEFS) 2>&1 || exit 1; \
	mqtt_tx_size=$$(wc -c < $(BUILD_DIR)/$(ZX_NAME)_MQTT_TX.OVL); \
	if [ "$$mqtt_tx_size" -gt 2048 ]; then \
		printf "[ERR] $(ZX_NAME)_MQTT_TX.OVL too large: $$mqtt_tx_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	$(ZX_Z80ASM) asm/overlay/direct/entry_direct.asm 2>&1 || exit 1; \
	(cd $(BUILD_DIR) && $(ZCC) +z80 $(ZX_OVL_CFLAGS) -c ../src/spectrum/overlay/direct_ovl.c -o direct_ovl.o) 2>&1 || exit 1; \
	rm -f $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~; \
	$(ZX_Z80ASM) -b -r0x$$SLOT -o=$(BUILD_DIR)/$(ZX_NAME)_DIRECT.OVL \
		asm/overlay/direct/entry_direct.o $(BUILD_DIR)/direct_ovl.o $(OVL_DEFS) 2>&1 || exit 1; \
	direct_size=$$(wc -c < $(BUILD_DIR)/$(ZX_NAME)_DIRECT.OVL); \
	if [ "$$direct_size" -gt 2048 ]; then \
		printf "[ERR] $(ZX_NAME)_DIRECT.OVL too large: $$direct_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	$(ZX_Z80ASM) asm/overlay/menu_config/entry_menu_config.asm 2>&1 || exit 1; \
	(cd $(BUILD_DIR) && $(ZCC) +z80 $(ZX_OVL_CFLAGS) -c ../src/spectrum/overlay/menu_config_ovl.c -o menu_config_ovl.o) 2>&1 || exit 1; \
	rm -f $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~; \
	$(ZX_Z80ASM) -b -r0x$$SLOT -o=$(BUILD_DIR)/$(ZX_NAME)_MENU_CONFIG.OVL \
		asm/overlay/menu_config/entry_menu_config.o $(BUILD_DIR)/menu_config_ovl.o $(OVL_DEFS) 2>&1 || exit 1; \
	menu_config_size=$$(wc -c < $(BUILD_DIR)/$(ZX_NAME)_MENU_CONFIG.OVL); \
	if [ "$$menu_config_size" -gt 2048 ]; then \
		printf "[ERR] $(ZX_NAME)_MENU_CONFIG.OVL too large: $$menu_config_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	$(ZX_Z80ASM) asm/overlay/menu_logic/entry_menu_logic.asm 2>&1 || exit 1; \
	(cd $(BUILD_DIR) && $(ZCC) +z80 $(ZX_OVL_CFLAGS) -c ../src/spectrum/overlay/menu_logic_ovl.c -o menu_logic_ovl.o) 2>&1 || exit 1; \
	(cd $(BUILD_DIR) && $(ZCC) +z80 $(ZX_OVL_CFLAGS) -c ../src/spectrum/overlay/status_ovl.c -o status_ovl.o) 2>&1 || exit 1; \
	rm -f $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~; \
	$(ZX_Z80ASM) -b -r0x$$SLOT -o=$(BUILD_DIR)/$(ZX_NAME)_MENU_LOGIC.OVL \
		asm/overlay/menu_logic/entry_menu_logic.o $(BUILD_DIR)/menu_logic_ovl.o $(BUILD_DIR)/status_ovl.o $(OVL_DEFS) 2>&1 || exit 1; \
	menu_logic_size=$$(wc -c < $(BUILD_DIR)/$(ZX_NAME)_MENU_LOGIC.OVL); \
	if [ "$$menu_logic_size" -gt 2048 ]; then \
		printf "[ERR] $(ZX_NAME)_MENU_LOGIC.OVL too large: $$menu_logic_size bytes (max 2048)\n"; \
		exit 1; \
	fi; \
	status_size=0; \
	ovl_tmp=$(BUILD_DIR)/$(ZX_NAME).OVL.tmp; \
	rm -f "$$ovl_tmp"; \
	dd if=$(BUILD_DIR)/$(ZX_NAME)_RULES.OVL of="$$ovl_tmp" bs=2048 conv=sync 2>/dev/null || { printf "[ERR] OVL pack failed: RULES\n"; exit 1; }; \
	dd if=$(BUILD_DIR)/$(ZX_NAME)_BOARD.OVL of="$$ovl_tmp" bs=2048 seek=1 conv=sync,notrunc 2>/dev/null || { printf "[ERR] OVL pack failed: BOARD\n"; exit 1; }; \
	dd if=$(BUILD_DIR)/$(ZX_NAME)_GUI_LOG.OVL of="$$ovl_tmp" bs=2048 seek=2 conv=sync,notrunc 2>/dev/null || { printf "[ERR] OVL pack failed: GUI_LOG\n"; exit 1; }; \
	dd if=$(BUILD_DIR)/$(ZX_NAME)_MQTT_CONNECT.OVL of="$$ovl_tmp" bs=2048 seek=3 conv=sync,notrunc 2>/dev/null || { printf "[ERR] OVL pack failed: MQTT_CONNECT\n"; exit 1; }; \
	dd if=$(BUILD_DIR)/$(ZX_NAME)_MQTT_TX.OVL of="$$ovl_tmp" bs=2048 seek=4 conv=sync,notrunc 2>/dev/null || { printf "[ERR] OVL pack failed: MQTT_TX\n"; exit 1; }; \
	dd if=$(BUILD_DIR)/$(ZX_NAME)_DIRECT.OVL of="$$ovl_tmp" bs=2048 seek=5 conv=sync,notrunc 2>/dev/null || { printf "[ERR] OVL pack failed: DIRECT\n"; exit 1; }; \
	dd if=$(BUILD_DIR)/$(ZX_NAME)_MENU_CONFIG.OVL of="$$ovl_tmp" bs=2048 seek=6 conv=sync,notrunc 2>/dev/null || { printf "[ERR] OVL pack failed: MENU_CONFIG\n"; exit 1; }; \
	dd if=$(BUILD_DIR)/$(ZX_NAME)_MENU_LOGIC.OVL of="$$ovl_tmp" bs=2048 seek=7 conv=sync,notrunc 2>/dev/null || { printf "[ERR] OVL pack failed: MENU_LOGIC\n"; exit 1; }; \
	dd if=$(BUILD_DIR)/$(ZX_NAME)_HINTS.OVL of="$$ovl_tmp" bs=2048 seek=8 conv=sync,notrunc 2>/dev/null || { printf "[ERR] OVL pack failed: HINTS\n"; exit 1; }; \
	cp -f "$$ovl_tmp" $(ZX_OVL) || { printf "[ERR] OVL generation failed: $(ZX_OVL)\n"; exit 1; }; \
	rm -f "$$ovl_tmp"; \
	ovl_total=$$(wc -c < $(ZX_OVL)); \
	printf '{"RULES":%s,"BOARD":%s,"GUI_LOG":%s,"MQTT_CONNECT":%s,"MQTT_TX":%s,"DIRECT":%s,"MENU_CONFIG":%s,"MENU_LOGIC":%s,"HINTS":%s,"STATUS":%s,"total":%s}\n' "$$ovl_size" "$$board_size" "$$gui_log_size" "$$mqtt_connect_size" "$$mqtt_tx_size" "$$direct_size" "$$menu_config_size" "$$menu_logic_size" "$$hints_size" "$$status_size" "$$ovl_total" > $(BUILD_DIR)/overlay_sizes.json; \
	printf "[OK] $(ZX_NAME).OVL: RULES $$ovl_size bytes, BOARD $$board_size bytes, GUI_LOG $$gui_log_size bytes, MQTT_CONNECT $$mqtt_connect_size bytes, MQTT_TX $$mqtt_tx_size bytes, DIRECT $$direct_size bytes, MENU_CONFIG $$menu_config_size bytes, MENU_LOGIC $$menu_logic_size bytes, HINTS $$hints_size bytes, STATUS $$status_size bytes, total $$ovl_total bytes\n"; \
	rm -f asm/overlay/rules/entry_rules.o asm/overlay/rules/rules_stub.o asm/overlay/hints/entry_hints.o asm/overlay/board/entry_board.o asm/overlay/board/helpers.o asm/overlay/gui_log/entry_gui_log.o asm/overlay/mqtt_connect/entry_mqtt_connect.o asm/overlay/mqtt_tx/entry_mqtt_tx.o asm/overlay/direct/entry_direct.o asm/overlay/menu_config/entry_menu_config.o asm/overlay/menu_logic/entry_menu_logic.o asm/overlay/status/entry_status.o $(BUILD_DIR)/asm/overlay/rules/rules_stub.o $(BUILD_DIR)/board_apply_ovl.o $(BUILD_DIR)/gui_log_ovl.o $(BUILD_DIR)/mqtt_connect_ovl.o $(BUILD_DIR)/mqtt_tx_ovl.o $(BUILD_DIR)/direct_ovl.o $(BUILD_DIR)/menu_config_ovl.o $(BUILD_DIR)/menu_logic_ovl.o $(BUILD_DIR)/status_ovl.o $(BUILD_DIR)/overlay_defs.o $(BUILD_DIR)/overlay_defs.o~ $(BUILD_DIR)/$(ZX_NAME)_RULES.OVL $(BUILD_DIR)/$(ZX_NAME)_HINTS.OVL $(BUILD_DIR)/$(ZX_NAME)_BOARD.OVL $(BUILD_DIR)/$(ZX_NAME)_GUI_LOG.OVL $(BUILD_DIR)/$(ZX_NAME)_MQTT_CONNECT.OVL $(BUILD_DIR)/$(ZX_NAME)_MQTT_TX.OVL $(BUILD_DIR)/$(ZX_NAME)_DIRECT.OVL $(BUILD_DIR)/$(ZX_NAME)_MENU_CONFIG.OVL $(BUILD_DIR)/$(ZX_NAME)_MENU_LOGIC.OVL $(BUILD_DIR)/$(ZX_NAME)_STATUS.OVL $(BUILD_DIR)/$(ZX_NAME).OVL.tmp 2>/dev/null || true

FORCE:

clean: clean-spectrum clean-client

clean-spectrum:
ifneq ($(CLIENT_HOST_IS_WINDOWS),)
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File tools/clean-generated.ps1 -SpectrumOnly
else
	rm -rf $(BUILD_DIR) dist
	rm -f src/spectrum/*.c.asm
	rm -f $(RELEASE_DIR)/$(ZX_NAME).tap $(RELEASE_DIR)/$(ZX_NAME).OVL $(RELEASE_DIR)/$(ZX_NAME).DAT
	rm -f $(RELEASE_DIR)/NCHESSZX.tap $(RELEASE_DIR)/NCHESSZX.OVL $(RELEASE_DIR)/NCHESSZX.DAT
	rm -f $(RELEASE_DIR)/$(ZX_NAME)_MQTT_W.tap $(RELEASE_DIR)/$(ZX_NAME)_MQTT_W.OVL $(RELEASE_DIR)/$(ZX_NAME)_MQTT_W.DAT
	rm -f $(RELEASE_DIR)/$(ZX_NAME)_MQTT_B.tap $(RELEASE_DIR)/$(ZX_NAME)_MQTT_B.OVL $(RELEASE_DIR)/$(ZX_NAME)_MQTT_B.DAT
	rm -f $(RELEASE_DIR)/MQTTW $(RELEASE_DIR)/MQTTB $(RELEASE_DIR)/MQTTWKEY $(RELEASE_DIR)/MQTTBKEY $(RELEASE_DIR)/ZXCHNET.tap
	rm -f asm/overlay/rules/entry_rules.o asm/overlay/rules/rules_stub.o
	rmdir $(RELEASE_DIR) 2>/dev/null || true
endif

clean-client:
ifneq ($(CLIENT_HOST_IS_WINDOWS),)
	$(POWERSHELL) -NoProfile -ExecutionPolicy Bypass -File tools/clean-generated.ps1 -ClientOnly
else
	rm -rf client/build client/build_manual client/build_qmake client/build_verify client/dist
	rm -rf $(RELEASE_DIR)/shatranj-client $(RELEASE_DIR)/shatranj $(RELEASE_DIR)/netchesszx-client
	rm -f main.obj
	rmdir $(RELEASE_DIR) 2>/dev/null || true
endif

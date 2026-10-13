import tempfile
import unittest
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import check_layering


class TimeOwnerGuardTests(unittest.TestCase):
    def violations_for(self, files):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for relative, contents in files.items():
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(contents, encoding="utf-8")
            return check_layering.find_violations(root, set())

    def test_rejects_time_state_outside_owner(self):
        violations = self.violations_for(
            {"src/spectrum/ui/gui.c": "static unsigned char clock_frames;\n"}
        )
        self.assertTrue(any(rule == "time-state-owner" for rule, *_ in violations))

    def test_allows_fixed_address_time_storage_declaration(self):
        violations = self.violations_for({"src/spectrum/app/time.h":
            "extern __at(0x5fec) unsigned char spectrum_time_move_frame_phase;\n"})
        self.assertFalse(any(rule == "app-no-extern-function-declarations"
                             for rule, *_ in violations))

    def test_space_dependencies_are_self_contained(self):
        for include in ("common/chess/position.h", "common/session/session.h",
                        "common/protocol/game_protocol.h", "spectrum/app/ux_ports.h",
                        "spectrum/ui/gui.h", "spectrum/transport/net.h",
                        "pc/client/board_widget.h", "QWidget", "arch/zx.h",
                        "../chess/position.h"):
            with self.subTest(include=include):
                violations = self.violations_for({
                    "src/common/space/rect.c": f'#include "{include}"\n'
                })
                self.assertTrue(any(rule == "space-self-contained"
                                    for rule, *_ in violations))
        allowed = self.violations_for({
            "src/common/space/rect.c": '#include "common/space/rect.h"\n',
            "src/common/space/space.h": '#include <stdint.h>\n'
        })
        self.assertFalse(any(rule == "space-self-contained" for rule, *_ in allowed))

    def test_cursor_only_depends_on_ids_and_navigation_port(self):
        for include in ("spectrum/app/ux_ports.h", "spectrum/ui/gui.h",
                        "common/chess/position.h", "spectrum/session/state.h"):
            violations = self.violations_for({
                "src/spectrum/ui/cell_cursor.c": f'#include "{include}"\n'
            })
            self.assertTrue(any(rule == "cell-cursor-ports-only" for rule, *_ in violations))
        allowed = self.violations_for({"src/spectrum/ui/cell_cursor.c":
            '#include "spectrum/ui/space_navigation.h"\n#include "common/space/space.h"\n'})
        self.assertFalse(any(rule == "cell-cursor-ports-only" for rule, *_ in allowed))

    def test_rejects_direct_time_state_write(self):
        for source in (
            "spectrum_time_state.clock_hour = 1u;",
            "spectrum_time_state.game_timers[0] = 99u;",
            "++spectrum_time_frames;",
            "spectrum_time_move_frame_phase = 2u;",
            "++spectrum_time_clock_frames;",
            "memset(&spectrum_time_state, 0, sizeof spectrum_time_state);",
            "uint8_t *alias = spectrum_time_state.game_timers;",
        ):
            with self.subTest(source=source):
                violations = self.violations_for({"src/spectrum/ui/gui.c": source})
                self.assertTrue(any(rule == "time-state-owner" for rule, *_ in violations))

    def test_allows_const_view_comparisons_and_operations(self):
        violations = self.violations_for({"src/spectrum/ui/gui.c":
            "if (spectrum_time_view()->clock_hour == 1u) {}\n"
            "if (spectrum_time_view()->game_timers[0] != 0u) {}\n"
            "spectrum_time_snapshot();\n"})
        self.assertFalse(any(rule == "time-state-owner" for rule, *_ in violations))

    def test_ui_cannot_drive_or_mutate_time(self):
        for source in (
            '#include "spectrum/app/time.h"',
            'SPECTRUM_TIME_TICK(dirty);',
            'spectrum_app_game_timer_start();',
            'spectrum_net_runtime_set_clock(1, 2, 3);',
            '#define WRAPPED_TICK() SPECTRUM_TIME_TICK(dirty)',
        ):
            with self.subTest(source=source):
                violations = self.violations_for({"src/spectrum/ui/gui.c": source})
                self.assertTrue(any(rule == "ui-time-read-only" for rule, *_ in violations))

    def test_rejects_asm_storage_access(self):
        violations = self.violations_for({"asm/spectrum/probe.asm":
            "EXTERN _spectrum_time_frames\nld (_spectrum_time_frames), a\n"})
        self.assertTrue(any(rule == "time-state-owner" for rule, *_ in violations))

    def test_rejects_ui_dependency_from_time_owner(self):
        violations = self.violations_for(
            {"src/spectrum/app/time.c": '#include "spectrum/ui/gui.h"\n'}
        )
        self.assertTrue(any(rule == "time-owner-no-ui" for rule, *_ in violations))

    def test_included_c_fragment_obeys_session_boundaries(self):
        for include, rule in (
            ("spectrum/ui/gui.h", "protocol-no-ui"),
            ("spectrum/board/board.h", "session-no-board"),
            ("spectrum/app/private.h", "session-no-app"),
        ):
            with self.subTest(include=include):
                violations = self.violations_for({
                    "src/spectrum/session/transitions.inc": f'#include "{include}"\n'
                })
                self.assertTrue(any(actual == rule for actual, *_ in violations))

    def test_transport_and_overlay_ports_are_scoped_to_their_owner(self):
        for owner, include, rule in (
            ("src/spectrum/transport/net.c", "spectrum/session/identity.h", "transport-no-session"),
            ("src/spectrum/transport/net.c", "spectrum/session/timing.h", "transport-no-session"),
            ("src/spectrum/transport/keepalive_protocol.c", "common/protocol/game_protocol.h", "transport-no-game-protocol"),
            ("src/spectrum/overlay/overlay.c", "spectrum/app/input_cmd.h", "overlay-no-app"),
        ):
            with self.subTest(include=include):
                source = f'#include "{include}"\n'
                allowed = self.violations_for({owner: source})
                self.assertFalse(any(actual == rule for actual, *_ in allowed))
                other = str(Path(owner).with_name("foreign.c")).replace("\\", "/")
                denied = self.violations_for({other: source})
                self.assertTrue(any(actual == rule for actual, *_ in denied))
        violations = self.violations_for({"src/spectrum/transport/net.h":
            '#include "spectrum/config/session.h"\n'})
        self.assertTrue(any(rule == "transport-no-config" for rule, *_ in violations))
        violations = self.violations_for({"src/spectrum/transport/net.c":
            "netchesszx_session_configure(0, 1, 0);"})
        self.assertTrue(any(rule == "transport-session-read-only" for rule, *_ in violations))
        violations = self.violations_for({"src/spectrum/transport/esp_at.c":
            "netchesszx_transport_is_mqtt();"})
        self.assertTrue(any(rule == "transport-mode-reader" for rule, *_ in violations))

    def test_config_writes_and_aliases_are_guarded_without_an_include(self):
        for source in (
            "netchesszx_timezone = 1;", "netchesszx_timezone += 1;",
            "++netchesszx_timezone;", "netchesszx_timezone--;",
            "(netchesszx_timezone) = 1;", "++(netchesszx_timezone);",
            "int8_t *alias = &netchesszx_timezone;",
            "char *alias = netchesszx_mqtt_code;",
            "#define SETTING (netchesszx_timezone)\nSETTING = 1;",
            "netchesszx_direct_host[0] = 0;",
            "memset(netchesszx_hinted_rows, 0, 8);",
            "memcpy(\nnetchesszx_mqtt_code, text, 5);",
        ):
            with self.subTest(source=source):
                violations = self.violations_for({"src/spectrum/ui/probe.c": source})
                self.assertTrue(any(rule.startswith("config-state-") for rule, *_ in violations))

    def test_config_owners_and_readers_have_different_permissions(self):
        check = check_layering.check_config_state
        self.assertFalse(check("src/spectrum/app/app.c", "netchesszx_timezone = 1;"))
        self.assertTrue(check("src/spectrum/app/app.c", "netchesszx_notation = 1;"))
        for source in ("netchesszx_timezone == 1;", "sizeof(netchesszx_hinted_rows);",
                       "if (netchesszx_hinted_rows[0] != 0) {}",
                       "// netchesszx_timezone = 1;\n", 'puts("netchesszx_timezone = 1;");'):
            self.assertFalse(check("src/spectrum/ui/probe.c", source))
        reader = "src/spectrum/overlay/status_ovl.c"
        self.assertFalse(check(reader, "const char *name = netchesszx_mqtt_code;"))
        for source in ("strcpy(netchesszx_mqtt_code, text);",
                       "memset(&netchesszx_mqtt_code[0], 0, 5);",
                       "memcpy((void *)netchesszx_mqtt_code, text, 5);"):
            self.assertTrue(check(reader, source))
        # Runtime hint masks are not configuration-record fields.
        self.assertTrue(check("src/spectrum/overlay/config_ovl.c", "netchesszx_hinted_rows[0] = 1;"))
        self.assertFalse(check("src/spectrum/app/app.c", "netchesszx_hinted_rows[0] = 1;"))

    def test_config_asm_writes_and_address_loads_need_explicit_owners(self):
        check = check_layering.check_config_state
        for source in ("ld (_netchesszx_timezone), a", "ld hl, _netchesszx_timezone",
                       "defc alias = _netchesszx_timezone"):
            self.assertTrue(check("asm/overlay/probe.asm", source))
        source = "ld (_netchesszx_timezone), a\nld (_netchesszx_timezone_last), a"
        self.assertFalse(check("asm/overlay/time_config/entry_time_config.asm", source))
        self.assertFalse(check("asm/overlay/probe.asm", "ld a, (_netchesszx_timezone)"))
        reader = "asm/next/board_hints_view.inc"
        self.assertFalse(check(reader, "ld hl, _netchesszx_hinted_rows"))
        self.assertTrue(check(reader, "ld (_netchesszx_hinted_rows), a"))

    def test_app_cannot_use_overlay_symbols_through_compat_headers(self):
        for source in ('spectrum_overlay_exec_cached(16, 2);',
                       'spectrum_overlay_context[0] = 1;',
                       'netchesszx_setup_overlay_context[0] = 1;',
                       '++netchesszx_setup_overlay_context[1];',
                       'read(netchesszx_host_overlay_context[2]);',
                       'read(NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR);',
                       'unsigned char entry = SPECTRUM_OVL_TIME_CONFIG_INIT;'):
            violations = self.violations_for({'src/spectrum/app/app.c': source})
            self.assertTrue(any(rule == 'app-no-overlay-dispatch' for rule, *_ in violations))
        port = ('spectrum_net_runtime_set_clock (\n'
                '  netchesszx_setup_overlay_context [ 2u ],\n'
                '  netchesszx_setup_overlay_context [ 1 ],\n'
                '  netchesszx_setup_overlay_context [ 0 ] );')
        for owner, source, denied in (
                ('app.c', port, False),
                ('other.c', port, True),
                ('app.c', port.replace('[ 2u ]', '[ 3 ]'), True),
                ('app.c', port.replace('[ 0 ]', '[ 0 ]++'), True),
                ('app.c', port.replace('set_clock', 'other_consumer'), True),
                ('app.c', port + '\nwrite(&netchesszx_setup_overlay_context[0]);', True)):
            violations = self.violations_for({f'src/spectrum/app/{owner}': source})
            self.assertEqual(any(rule == 'app-no-overlay-dispatch' for rule, *_ in violations), denied)
        for owner, include, expected in (
                ('src/spectrum/saveload/saveload.h', 'spectrum/overlay/overlay.h', 'saveload-public-no-overlay'),
                ('src/spectrum/overlay/status_ovl.c', 'spectrum/transport/esp_at.h', 'overlay-no-esp-at')):
            violations = self.violations_for({owner: f'#include "{include}"'})
            self.assertTrue(any(rule == expected for rule, *_ in violations))

    def test_fixed_context_aliases_match_authority(self):
        for owner, name in (('asm/overlay/time_config/entry_time_config.asm', 'CTX'),
                            ('asm/overlay/menu_config/entry_menu_config.asm', 'CTX_KEY'),
                            ('asm/next/screen.asm', 'NETCHESSZX_OVERLAY_CONTEXT')):
            for actual in ('0x5fe0', '0x5fe1'):
                violations = self.violations_for({
                    'src/spectrum/lowram_map.h': '#define NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR 0x5fe0u',
                    owner: f'{name} EQU {actual}'})
                errors = [v for v in violations if v[0] == 'asm-lowram-map-match' and v[1] == owner]
                self.assertEqual(bool(errors), actual != '0x5fe0')

    def test_timer_buffer_alias_checks_both_target_branches(self):
        owner = 'asm/spectrum/timer_roll_layout.inc'
        header = ('#ifdef NETCHESSZX_NEXT_BANKING\n'
                  '#define NETCHESSZX_LOWRAM_TIMER_ROLL_ADDR 0x3f18u\n'
                  '#else\n#define NETCHESSZX_LOWRAM_TIMER_ROLL_ADDR 0x67cbu\n#endif\n')
        for classic, next_ in (('0x67cb', '0x3f18'), ('0x67cc', '0x3f18'), ('0x67cb', '0x3f19')):
            source = (f'IFDEF NETCHESSZX_NEXT_BANKING\ntimer_roll_buffer EQU {next_}\n'
                      f'ELSE\ntimer_roll_buffer EQU {classic}\nENDIF\n')
            violations = self.violations_for({'src/spectrum/lowram_map.h': header, owner: source})
            errors = [v for v in violations if v[1] == owner and v[3].startswith('timer_roll_buffer')]
            self.assertEqual(len(errors), int(classic != '0x67cb' or next_ != '0x3f18'))

    def test_player_identity_shape_follows_the_mname_protocol(self):
        protocol = ('#define NETCHESS_PROTO_NAME_PREFIX_LEN 6u\n'
                    '#define NETCHESS_PLAYER_NAME_MAX 10u\n')
        header = ('#ifdef NETCHESSZX_NEXT_BANKING\n'
                  '#define NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_ADDR 0x3f2au\n'
                  '#else\n#define NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_ADDR 0x65d9u\n#endif\n'
                  '#define NETCHESSZX_LOWRAM_PLAYER_NAME_LINE_SIZE {line}u\n'
                  '#define NETCHESSZX_LOWRAM_PEER_NAME_SIZE {peer}u\n')
        layout = ('IFDEF NETCHESSZX_NEXT_BANKING\nPLAYER_NAME_LINE EQU 0x3f2a\n'
                  'ELSE\nPLAYER_NAME_LINE EQU 0x65d9\nENDIF\n'
                  'PEER_NAME        EQU PLAYER_NAME_LINE + {peer_off}\n'
                  'PLAYER_NAME      EQU PLAYER_NAME_LINE + {name_off}\n'
                  'PLAYER_NAME_MAX  EQU {name_max}\n')
        good = dict(line=17, peer=11, peer_off=17, name_off=6, name_max=10)
        for change in ({}, {'name_max': 11}, {'name_off': 7}, {'peer_off': 16},
                       {'line': 18, 'peer_off': 18}, {'peer': 12}):
            values = dict(good, **change)
            violations = self.violations_for({
                'src/common/protocol/platform_protocol.h': protocol,
                'src/spectrum/lowram_map.h': header.format(**values),
                'asm/app/player_name_layout.inc': layout.format(**values)})
            errors = [v for v in violations if v[0] == 'asm-lowram-map-match' and
                      v[1] in ('asm/app/player_name_layout.inc', 'src/spectrum/lowram_map.h')]
            self.assertEqual(bool(errors), bool(change), (change, errors))

    def test_new_mutable_config_exports_require_a_policy(self):
        check = check_layering.check_config_state
        self.assertTrue(check("src/spectrum/config/session.h", "extern uint8_t netchesszx_new_option;"))
        self.assertFalse(check("src/spectrum/config/session.h", "extern const uint8_t netchesszx_constant;"))


if __name__ == "__main__":
    unittest.main()

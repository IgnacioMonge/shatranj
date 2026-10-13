# Z80 Optimization and Evidence Ledger

Status: completed software and physical-evidence ledger, 2026-08-17.
Measurements come from linked Classic/Next artifacts and the recorded physical
results; estimates are never added together. H1 and H3 passed on hardware.
H2 was physically characterised and closed as unsupported pre-session traffic,
not left pending.

## Preserved behaviour and hard limits

- Preserve protocol, wire/save formats, overlay ABI, AT command ordering,
  visible strings, chess legality, and interrupt/MMU ownership.
- The FILEUI S4 experiment may intentionally switch the displayed timestamp
  to the FAT timestamp supplied by the directory entry.
- Every overlay must remain at or below 2048 bytes.
- The linked SP gap must remain at least 512 bytes; 768 bytes is the warning
  threshold.
- Do not move overlay-only code resident and do not update baselines merely to
  accept growth.

## Measured result

| Target | Fresh audit resident | Final resident | Net | Final SP gap | Tightest final overlay |
| --- | ---: | ---: | ---: | ---: | --- |
| Classic | 34858 | 34739 | -119 | 1547 | RESTORE 1926/2048 |
| Next | 35596 | 35479 | -117 | 707 | MQTT_CONNECT 1997/2048 |

S6/S8/S9/S10 save 122 resident bytes on both targets. The retained IM1
hardening costs two bytes on Next, hence the optimization block's net -120.
The integrated main includes three additional resident bytes from intervening
protocol commits, producing the table totals above. S4 and S5 additionally
save 315 and 107 bytes in their overlays on both targets. The RTC year predicate
is unchanged and now has explicit tests for 2024, 2035, and 2036.

## Option ledger

| ID | Candidate | Acceptance | Status / measured result |
| --- | --- | --- | --- |
| S1 | Compact Next MQTT_CONNECT | Preserve AT behaviour; save at least 50 overlay bytes; zero resident cost | Rejected and reverted after two measured designs. Fixed table/probe saved 7 then 17 bytes total. A distinct `at_cmd_med` helper passed focused tests but saved only 13 bytes Classic and 18 bytes Next (1997 -> 1979), below the 50-byte rule. |
| S2 | Pack `gui_msg_blob` | Preserve all text; save overlay bytes; scratch <=26 bytes | Rejected and reverted. Six-token expander passed text tests but grew GUI_LOG 1889 -> 1953 (+64 bytes). Decoder cost exceeded blob saving. |
| S3 | Arithmetic RESTORE base64 | Preserve NCZS wire/CRC; save overlay bytes | Rejected and reverted. Arithmetic URL-safe mapper passed wire/invalid-boundary tests but grew RESTORE 1926 -> 1996 (+70 bytes). SDCC branch cost exceeded the 65-byte table. |
| S4 | Use FILEUI dirent FAT stamp | Preserve list/save/load; save overlay bytes; record visible timestamp change | Accepted: FILEUI 1913 -> 1598 (-315 bytes) on Classic and Next. Filename timestamp decoder/state removed; rows now use the esxDOS dirent FAT stamp promised by the file contract. Classic emulator saved `GAME1`, listed `17-08-26 02:51`, completed RQ/RY + RS00/RS01 + RA, and returned to the board with `RESTORED`. |
| S5 | Merge white/black RULES twins | Perft, castling, EP, hints green; save overlay bytes | Accepted: shared parameterized pawn/castling paths reduce RULES 1903 -> 1796 (-107 bytes). Board tests, compact perft, and Python oracle (205 move sets, 416 perft checks) pass. |
| S6 | Pool three duplicated resident strings | One linked copy each; 50 bytes | Accepted: 50 bytes on Classic and Next |
| S7 | Replace `strcmp` library pull | Preserve five comparisons; remove `asm_strcmp`; save resident bytes | Rejected and reverted. Portable+ASM equality helper passed protocol tests but its body was about 36 bytes versus the existing 23-byte `strcmp` pull, before call-site effects. |
| S8 | Factor moves/chat list/address ASM | Preserve rendering; save resident bytes | Accepted on narrower second design: shared move/chat single-row address/Y setup saves 28 linked resident bytes after subtracting the exact S9/S10 spans. Classic emulator rendered a local move/ACK and remote `CHAT hello` through the shared paths. |
| S9 | Share `net_copy`/`net_move` load | Preserve transport copies; save resident bytes | Accepted: `net_copy` tail-jumps to the overlap-safe `net_move`; both public symbols remain. Exact linked span 25 -> 2 (-23 bytes); ASM vectors and network/session tests pass. |
| S10 | Share token/digit load | Preserve parsers; save resident bytes | Accepted: carry selects token/digit after one shared packed-argument prologue. Exact linked token-to-copy-rest span 112 -> 91 (-21 bytes); updated mutation vector and protocol tests pass. |
| S11 | Establish IM1 in Next hard reset | Remove the CRT/NEX-loader IM-mode assumption before `EI; HALT`; preserve reset timing | Accepted: exact CRT disassembly showed IM1 only on the CRT exit path, so `_net_uart_hard_reset` now executes `IM 1` while DI before its first EI. Cost: +2 Next resident bytes; no Classic cost. Boot expansion was traced restoring MMU0/MMU1/MMU2 before EI. |

## Evidence backlog

| ID | Evidence task | Completion rule | Status |
| --- | --- | --- | --- |
| E1 | Reliable `.c.asm`/`.lst` output | Inspect exact generated code for the questioned inline ASM | Accepted during the audit: exact z88dk 2.4 generated CONNECT and TX listings; TX proved BC/DE are reloaded after RTC inline ASM and no compiler temporary is live across the block. The disposable listing generator and artifacts were removed after the audit at the user's request. |
| E2 | Exact CI toolchain | `make full-check` with `z88dk/z88dk:2.4` | Accepted: exact z88dk 2.4 full gate passes; final linked sizes are recorded above. |
| E3 | Emulator acceptance | Classic/Next boot plus affected DIRECT/UI/load paths | Accepted for emulator scope: current staged TAP and NEX boot. Classic DIRECT completed handshake/start, `MOVE 1 e2e4`/`ACK 1 e4`, CHAT rendering, and FILE list/save/load. The virtual modem suite passes 9/9. Physical H1/H3 passed and H2 was characterised and closed. |
| H1 | NextZXOS `$5B00` RTC clobber probe | Real Next UART ring remains coherent across RST 8 `0x92`/`0x8E` | **Physical PASS, 2026-08-17:** key 1 reported `H1 RTC/RING PASS`, `CHANGED 0000`, FAT date `$5C11`, time `$4EA0`, service 1 (`0x92`). The exact `$5B00..$5BFF` canary remained intact. |
| H2 | RX during long DI | Characterise cold-load RX and decide whether preservation is a product requirement | **Closed as an architectural boot behaviour, 2026-08-17.** A physical 800-fps run received `$09A3` frames and reported 2 breaks/3 malformed frames across the exact expansion; another 800-fps run could not establish a clean pre-expansion baseline. Byte preservation is therefore not guaranteed under a synthetic cold-load flood. This is not a reachable session failure: expansion runs only before connection setup, after which production calls `spectrum_net_start_uart`, discards pending RX for 25 frames, and runs Next-specific command/baud recovery before creating a session. BridgeZX + RESET recovered without a power cycle. Adding a resident ring/pump would spend scarce Next memory for an unsupported pre-session stream, so it is rejected. Diagnostic logs and fixtures were removed after recording this result. |
| H3 | Hard-reset/dirty-link recovery | Physical ESP reset, transparent recovery, and reconnect succeed | **Physical PASS, 2026-08-17:** ESP/local UART recovered from 230400 to 115200; transparent recovery succeeded and the Mac received the test payload from the real Next; physical reset reported `PASS` and `AT VERIFIED` on all 3/3 repetitions. Diagnostic logs and fixtures were removed after recording the result. The IM1 prerequisite is S11. |

## Retained constraints and tradeoffs

- Next remains tight: SP gap is 707 bytes (hard floor 512, warning below 768)
  and MQTT_CONNECT is 1997/2048. This is not discarded; it is the reason every
  future resident or CONNECT change still needs exact multi-target sizing.
- Classic keeps the overlay slot at `$6800` and ZX data origin at `$7000` in
  contended RAM. The resident UART read path is above `$8000`; overlay polling
  and RST 8 do not introduce `EI; HALT`. Moving this architecture would cost
  more memory and is not justified by a reproduced failure.
- RTC dates 2024..2035 use the Next RTC path. From 2036 the existing predicate
  deliberately rejects the FAT date and falls back to SNTP; the misleading
  comment was fixed, but the behavior and wire format were preserved.
- No committed size/ABI baseline was loosened. Generated maps, listings, TAP,
  OVL, DAT, and NEX remain build artifacts rather than edited source.

## Closed audit leads

The stack/carry/callee-pop/IY/UART-NUL/MMU/pump/render/FRAMES candidates and
the RTC/About/FILEUI-slot/SEND-OK/buffer/NACK+SYNC/flip-hints second-pass leads
were traced and rejected as defects. They require no production change unless
new runtime evidence contradicts the audit.

## SAN ASM / streq experiment (2026-08-21)

Disposable worktree only (`Temp/shrink-ncx/wt-2b3e6c4a`), detached `1189095`
plus the uncommitted IY wraps. Not on `main`. `docs/size_report.baseline.json`
is stale versus HEAD+IY; A/B used a same-rev IY-only build.

| Target | IY-only CODE_FULL | After SAN+streq | Δ | SP_GAP Δ | OVL |
| --- | ---: | ---: | ---: | ---: | ---: |
| Classic | 35357 | 35057 | -300 | +300 | 0 |
| Next | (same SAN object) | resident 35329 | -303 vs IY 35632 | +300 | 0 |

SAN overlay-move (old S1) rejected: `is_legal_move_coords` loads RULES.

P0: `pop hl` after `_spectrum_board_is_legal_move_coords` clobbered L. Fixed
by saving L before the pops. Locked by `tests/spectrum/san_vector.asm` (g1f3,
four rook forms, suffix, fail NUL) plus a mutant that restores the clobber.

| ID | Item | Decision |
| --- | --- | --- |
| SAN-IX | Dead `push ix`/`pop ix` | Done. Function never uses IX; 6 code bytes. |
| SAN-CELLS0 | Discarded first `board_cells` in disambig | Done. 5 code bytes. 64× loop call left (speed, ~0 size). |
| SAN-LEN | ASM returns 1, C returns `san_len` | Documented in `san.h`. Callers only test `!= 0`. |
| SAN-NUL | No `out[0]='\\0'` before parse | Done. 2 code bytes; fail path now matches C. |
| SAN-ADD | `san.asm` untracked | Deferred until an explicit land-on-branch request. |

## Reproducible-toolchain recovery (2026-09-15)

The local z88dk frontend now matches revision `762578732c`, and both firmware
configuration stamps include the effective `zcc` and `z80asm` identities. This
prevents a toolchain switch from reusing stale linked artifacts. The first clean
comparison exposed a real Classic overage of 8 resident bytes and 3 CONTROL
bytes; the committed baseline was not changed.

Preserved behaviour: protocol classification and wire text, editor/menu output,
overlay entry ABI, save data, and visible UI. Acceptance requires the complete
Classic/Next ABI and size gate to pass against the existing baselines.

| ID | Candidate | Decision / focused evidence |
| --- | --- | --- |
| R1 | Fingerprint `zcc`/`z80asm` in build stamps | Accepted. A changed executable now invalidates Classic and Next firmware caches. |
| R2 | Remove CONTROL's duplicate capability flags | Accepted. Existing ACK/NACK/payload events encode the same distinction; session-event tests pass and CONTROL is 1034 bytes, 35 below baseline. The Classic-only return stub also writes only the ABI-defined byte in L. |
| R3 | Remove the menu transition's dead A-to-B-to-A save | Accepted: -2 resident bytes; 322 executable timer/menu cases pass across Classic and Next. |
| R4 | Rewind the editor context pointer instead of reloading its address | Accepted: -2 resident bytes; all 9 append cases pass. |
| R5 | Keep private edit row/column adjacent and load BC as a pair | Accepted: -4 resident bytes; the edit-field executable vector passes. |

## Menu restore-from-child (2026-09-16)

Leaving a rolling group after a child was shown used the focus index as the
old label id, so the first frames animated from a sibling (DISCN instead of
RESET on LINKS). `ld e,b` restores the child id after `taboption_focus_root`
clobbers E. Measured Classic +1 resident (35727 -> 35728, SP gap 550 -> 549);
the executable controller cases now record the packed roll spec. Overlay
ceilings were not tightened.

## Menu NTWRK/OFFER and THEME/HINTS (2026-09-16)

Rolling group labels LINKS/MATCH became NTWRK/OFFER. THEME gained HINTS as a
third child that xors `netchesszx_movement_hints` in RAM (same lifetime as a
menu theme/piece change; Setup save still writes the live flag). UI DAT grew
7 bytes (882 -> 889), so Classic/Next MQTT stream bases moved +7. Measured
Classic +29 resident (35728 -> 35757, SP gap 549 -> 520) and DAT 7218 -> 7225.
Overlay ceilings were not tightened.


## SAN and journal batching (2026-09-22)

Approved implementation following the read-only optimize-z80 scan. Preserve
SAN strings, STJ1 bytes, full validation before LOAD staging, SAVE read-back,
sync/close failures, replay rows/ply and raw UART service. No overlay nesting,
new resident buffers, format/ABI changes or SpectraNext block-size changes.
The frozen baseline is the current dirty tree at `9f440df` with z88dk
`762578732c`: Classic resident 35597, BSS 419, SP gap 599, JOURNAL 1923,
SAVELOAD 1573; Next resident 32568, BSS 517, SP gap 3611, SAVELOAD 2868.

| ID | Option and evidence | Acceptance / rollback | Decision |
| --- | --- | --- | --- |
| O1 | Hoist SAN's stable board getter; current scan calls it 63 times | Exact SAN and ABI; fewer linked resident bytes, no BSS, bounded stack; revert on mismatch or no saving | Accepted; measured below |
| O2 | Use existing 160-byte scratch: STJ header76 + chunk84; append chunk160 | Same files/state/failures, unchanged code-size class and stack; guards around scratch and read/write counts; revert on corruption or service regression | Accepted; measured below |
| O3 | Batch Classic replay, currently one F_READ per two-byte record | Same rows/ply/fallback, drain per record, fewer reads; at most the approved 40–80 code bytes plus 3 local bytes in SAVELOAD, zero resident growth | Accepted; measured below |

Rejected: remove LOAD validation, SAVE read-back, sync, or UART pumps. Those
changes invalidate persistence or reception contracts. Hardware SD/UART timing
remains a separate validation requirement; fake-esxDOS counts are not wall time.


All three options accepted. The current production links, built from the same
frozen source and unchanged configuration/toolchain, give:

| Target | Resident before -> after | BSS | SP gap before -> after | Changed overlays |
| --- | --- | --- | --- | --- |
| Classic | 35597 -> 35578 | 419 | 599 -> 599 | SAVELOAD 1573 -> 1572; JOURNAL 1923 unchanged |
| Next | 32568 -> 32549 | 517 | 3611 -> 3630 | SAVELOAD 2868 unchanged |

The linked SAN disambiguation block is 129 -> 110 bytes on both targets.
The scratch block changes add no code or reserved RAM. Classic replay shares
its read setup with adoption and uses saved BC/HL instead of three new state
bytes. Its extra transient stack use is four bytes; SAN uses two extra bytes.
These are local call-depth costs, not a measured whole-program stack high-water.
Classic's fixed IM2 reservation keeps its SP gap at 599; the resident saving
increases free space below that reservation. No size/ABI allowance was raised.

Measured F_READ/F_WRITE counts for a 400-byte body (including fixed header/EOF
operations in the exercised paths): LOAD 41 -> 17, SAVE with a live handle
44 -> 20, first append adopting NEW 28 -> 8. Replay of 200 records uses 201 -> 4
reads with 202 raw UART drains on both sides. File/snapshot/visible-row hashes and
fd/pend/handle state match the baseline. These figures measure syscall counts,
not SD latency or network throughput.

Validation: focused SAN and journal vectors, old-SAN failure of the getter
budget test, return-value mutation, chunk boundaries, even/odd short reads,
first/refill/EOF read errors, sync/close failures, scratch guards and balanced
SP; two Astra reviews. The first `make full-check` passed the host suite and
ABI checks but found SAVELOAD three bytes above baseline. Reusing F_READ's BC
result removed four bytes; journal vectors passed again. The closing
`make full-check -o test` reused the passing host suite and passed Classic/Next
linked ABI, size, BOARD entry and IY checks. Evidence and artifact hashes are in
`build/optimize_size_deltas.json`; logs: `build/optimize-full-check.log` and
`build/optimize-closeout-retry.log`.
Next's source binding was then refreshed to match the primary checkout's raw
bytes; Next ABI/size and linked checks passed again. Both final atlas bindings
are checked against the frozen source. Log: `build/optimize-next-refresh.log`.
SpectraNext retains 32-byte blocks; no SpectraNext firmware was built. Real
SD/UART timing remains pending; next hardware check is LOAD/SAVE/replay of
a 200-record game while RX is active, checking delivery and input responsiveness.

## Aggressive shrink session (2026-10-09)

Base `03ea62c`, measured commit against commit with one z88dk
(v24860-762578732-20260915, macOS) against a baseline frozen at that base;
the committed baseline was not used for deltas. Classic BSS end stays at
0xFD63 because of the fixed IM2 reservation, so Classic savings show as free
space below 0xFCFF; Next savings show as SP gap.

| Target | Resident before -> after | SP gap before -> after | Changed overlays |
| --- | --- | --- | --- |
| Classic | 35760 -> 35208 (-552) | 669 -> 669 | CONTROL 2034 -> 1923; RECOVERY 2004 -> 1853; RESTORE 1885 -> 1794 |
| Next | 32960 -> 32407 (-553) | 3223 -> 3776 | CONTROL 2242 -> 2131; RECOVERY 2220 -> 2069; RESTORE 1885 -> 1794 |

Acceptance rule: a change stays only if it saves its threshold on both
targets (20 B for L1/L6, 15 B per L3 site) and no limit gets worse.

| ID | Change | Classic / Next | Decision |
| --- | --- | --- | --- |
| L1 | RECOVERY CAPTURE reads the live position (staged at overlay scratch +73: RCV_TMP on Classic, unused scratch on Next); checkpoints and SAVE no longer load BOARD or build a 67 B stack snapshot; capture API loses its snapshot argument | resident -46 / -46, RECOVERY -60 | accepted |
| L6a | Base64 encode as one sextet loop | RECOVERY -91, RESTORE -91 | accepted |
| L6b | CONTROL classifies MOVE/CHAT/CANCEL/BYE/BUSY from a token table | CONTROL -111 | accepted |
| L3a | Spectrum ACK/NACK call `netchess_proto_parse_ply_tail()`; wrappers host-only | -85 / -85 | accepted |
| L3b | `netchess_text_is()` as an ASM mode of `after_prefix` (PING, ACK PING, guest HELLO) | -58 / -58 | accepted |
| L3c | DIRECT role parser rejects a null owner; owner parsers become one-liners | -15 / -15 | accepted |
| L3d | `main` sends five cancel paths to one Setup exit | -59 / -59 | accepted |
| L3e | One ACK/NACK move-reply branch in the session handler (frame 15 B) | -35 / -35 | accepted |
| L3f | `local_turn_now()` replaces three inline turn compares | -75 / -75 | accepted |
| L3g | Local input gates jump to shared `ok`/`cursor_ok` tails | -105 / -105 | accepted |
| L3h | `SPECTRUM_OVL_CTX_SET16()` word store for 20 pointer handoffs | -74 / -75 | accepted |
| L3i | `process_local_key` shared suppress-key tails | -15 / -8 | rejected (Next below 15) |
| L3j | `gui_board_cell` vs `spectrum_board_cell` (42 B twin) | not built | rejected: `ui-no-board` layering |
| L4 | Table dispatch in `session_control_handle_event` | not built | rejected: 16 compares bound it at ~128 B < 150 B |
| L5 | callee/fastcall conversions | not built | skipped: L1-L3 already exceed 250 B |

Rejected before the session, with evidence from `8841c0c`: function-level
dead code (4 candidates, 24 B), remaining tail calls (one pair), z88dk
library pull (48 B), moving cold Setup/main code to overlays (needs nested
overlays), global `--sdcccall` (rejected by this z88dk).

Ranked for a later plan: an SDCC peephole (with label-refcount and flag
liveness conditions) for the 42 resident `jr nz,+4; ld a,1; jr +1; xor a`
booleanizations (~84-126 B); building the HELLO/GAME START HOST/GUEST
strings from shared prefixes (~70 B, touches wire builders); the
`SESSION_LOAD_PREPARED` twin in load/restore (~30 B); overlay-side 16-bit
context reads (overlay margin only); the session dispatch restructure.

Behaviour: SAVE payloads are byte-identical (golden Z80 vector, run with
the Classic and Next scratch layouts). After an
apply, the checkpoint records the position on the board, not the decoded
copy; they differ only if BOARD failed to load during the apply. Validation:
focused host and Z80 vectors per change, `make test`, `make client-test`
(17/17), `make module-guards`, and one closing `make full-check`. No
hardware run yet; next hardware check is a checkpoint, SAVE and RESTORE
round trip on a real Classic and Next.

## Per-ply overlay loads (2026-10-10)

Traced with `tools/emu_ply_trace.py` (ZXESPEmu, Classic, 16 plies, SAN, the
Spectrum as white DIRECT host). Counts are exact; ZEsarUX serves SD reads
instantly, so time is not measured, and its esxDOS cannot create the journal
file (open mode 0x0F), whose write happens inside the SAVELOAD visit.

| Ply | Base `6d2ea35` | After `aa0453c` |
| --- | --- | --- |
| Remote MOVE | DIRECT CONTROL RULES BOARD DIRECT RULES GUI_LOG SAVELOAD RECOVERY (9) | DIRECT RULES BOARD DIRECT RULES GUI_LOG SAVELOAD RECOVERY (8) |
| ACK of a local move | DIRECT CONTROL BOARD RULES GUI_LOG SAVELOAD GUI_LOG RECOVERY (8) | DIRECT CONTROL BOARD RULES GUI_LOG SAVELOAD RECOVERY (7) |

| ID | Change | Classic / Next resident | Decision |
| --- | --- | --- | --- |
| E2 | Resident classifier names MOVE, like CHAT, before CONTROL | +21 / +21; Next 0 since E2b | accepted |
| C9 | Opponent's-turn notice shown while GUI_LOG is loaded | +17 / +17 | accepted |
| E2b | Also name `ACK <ply>` resident, Classic only (E2 too) | +53 / -21 | accepted 2026-10-11: one SD read (~30 ms) less per local move; Next and SpectraNext switch overlays without SD |
| C3 | One Classic loader retry, then ending the session like a lost link ("OVL load failed") when a BOARD state change still cannot load: both takeback paths, received RESTORE, accepted RESTORE; an offline LOAD only reports LOAD FAIL | +92 / +88 | accepted 2026-10-11 (user chose both). The reader keeps one RST 8 per request; a fault-injected `ovl_load` vector covers retry and give-up; host tests inject a BOARD failure into both takeback paths and an accepted RESTORE (no apply, no checkpoint, named notice); the normal 16-ply trace is unchanged. Resume afterwards works when the failing Spectrum is the host. Deferred: NACK/RN instead of the cut where the local apply precedes the reply (takeback accept, received RESTORE). Follow-up: the BOARD-loss handling is now Classic-only (`SPECTRUM_BOARD_RESTORE_CAN_FAIL` in `board.h`); Next and SpectraNext, whose overlays are preloaded pages, compile the unchecked pre-C3 apply. `make full-check` on Windows (z88dk `762578732c`): Classic resident 35387 B (the macOS 35394 minus the usual 7 B, so unchanged), Next resident 32412 B and SP_GAP 3771 against macOS 32515/3668 with C3; ABI unchanged. SpectraNext not yet measured |
| C10 | Cache the slot the last checkpoint wrote and read back (3 B resident data, Classic and Next) | +3 / +3; RECOVERY +44 / +44 | accepted 2026-10-11: 16-ply trace opens 64 -> 33, reads 97 -> 36; a failed write forgets it. Killing the emulator after ply 16 leaves slot 0 = generation 17, ply 16 and slot 1 = 16, ply 15; the product resume ASM (`tests/tools/test_recovery.py` harness) stages ply 16 from them |
| E3 | DIRECT #30 option (b), rely on CTS | not built | rejected 2026-10-11: blocks the send (backlog #30) |

On a real Classic (2026-10-11, `docs/hardware-validation.md`) the guest's
MOVE ACK latency fell from a 2.058 s median (`6d2ea35`) to 1.987 s
(`3cd2097`), and to 1.996 s (mean 1.988 s) with E2b (`718ee3b`). That
window holds the ACK of the Spectrum's own move and the guest's MOVE, which
lost C9's and E2's loads, then E2b's: one ~2 KB overlay read costs about 30
ms there, so a ply's 6-8 loads spend about 0.2 s on SD reads. Each checkpoint opens both `.RCV` slots
(4 opens, 6 reads, 1 write and fsync plus a readback). Validation: the new hot-classification vector and the
updated move-sequence vector in `tests/tools/test_classic_overlay_dispatch.py`,
session parity, and one closing `make full-check`. With E2b the ACK of a
local move is DIRECT BOARD RULES GUI_LOG SAVELOAD RECOVERY (6): the
16-ply trace loads CONTROL only during setup (MNAME, GAME START).

## Common Y/N deadlines (2026-10-12)

Preserve board/clock/history ownership, the one-operation invariant, real
link-loss handling, RESTORE transfer safety and target memory/ABI limits.
Acceptance: all four approval requests get 60 seconds of elapsed time;
ordinary TAKEBACK withdrawal rejects without undoing or ending the session.

| Option | Evidence / cost | Risk / verification | Decision |
| --- | --- | --- | --- |
| Reuse RN and CANCEL DRAW/RESET; correlate TAKEBACK withdrawal by ply | Existing restore/cancel paths; one new wire command and common ply parser | Duplicates, retained input and an ACK crossing cancellation; shared DIRECT/MQTT and Qt prompt tests | Do |
| Reuse the hardware frame counter for approval time | Existing atomic FRAMES view and 50/60 Hz selection; scheduler 6 -> 7 stack bytes, no new BSS/arena | Busy iterations and 16-bit wrap; focused production-coordinator test and shared parity | Do |
| Store the approval deadline beside the stack scheduler; share TAKEBACK/CANCEL ply validation with the existing unary fastcall ABI | First countdown helper occupied 305 B; first link was 36007 B Classic with 227 B overlapping the fixed frame vector, Next 33040 B / SP_GAP 3143 | Same frame-boundary and protocol tests; final links meet unchanged limits | Do |
| Convert transport retries to frame deadlines too | Absolute-timer candidate: Classic 35830 B still overlapped the fixed vector by 50 B; Next 32881 B / SP_GAP 3302 | Separate from the requested human deadline; retain existing retry behavior and cost | Defer |
| Increase empty-poll counts | No reliable wall-time bound when traffic/IO changes | Would retain the reported timing drift | Reject |
| Clear an expired TAKEBACK locally and ignore later ACK | No wire work | A peer can already have undone; leaves different boards | Reject |
| Add a general transaction framework or per-user timeout settings | More protocol/state/UI and migration cost | Outside the four requested decisions | Reject |

Final `make full-check` passed on Windows with z88dk `762578732c`:

| Measurement | First candidate | Final | Committed constraint |
| --- | --- | --- | --- |
| Classic resident | 36007 B (failed overlap) | 35739 B | 35753 B maximum |
| Next resident / SP_GAP | 33040 / 3143 B | 32770 / 3413 B | SP_GAP >= 3072 B |
| Scheduler stack storage | 9 B | 7 B | Previously 6 B; no persistent growth |

Classic BSS remains 415 B and SP_GAP 669 B; CONTROL is 1935 B against the
2034 B baseline and 2048 B slot. Next BSS is 513 B, CONTROL 2143 B against
its 4096 B limit. A final 35754 B Classic candidate exceeded the budget by
one byte; validating zero ply once before comparing the two latches removed
15 B on both targets, preserving the rejection path. Memory budgets were
unchanged; only the new parser export was added to both ABI manifests.
The original local maps were from 2026-10-06, so these baseline comparisons
include earlier pending changes and are not a measured delta from parent HEAD.

Validation: shared DIRECT corpus, MQTT 78/78 on Classic/Next at 50/60 Hz,
focused production-frame and grammar tests, `make client-test` (18/18), and
the closing full gate including linked BOARD/IY checks. Next validation:
physical Classic/Next expiry of all four requests at 50/60 Hz, crossed
TAKEBACK ACK, and dynamic stack high-water. SpectraNext remains unbuilt.

## Live-link cancellation repair (2026-10-12)

Acceptance: 60 real seconds, unchanged board on an unanswered request, live
DIRECT/MQTT connection after lost CANCEL/NACK and more than five retries,
continued MOVE/ACK and a new request, and a crossed accepted undo applied once
on both peers. Retain real link-loss/transfer failure, wire verbs, save formats,
overlay ABI and existing size limits. No new Spectrum BSS or scheduler storage.

| Option | Evidence / cost | Risk and check | Decision |
| --- | --- | --- | --- |
| Delete cancellation state on expiry | Loses the accepted undo whose ACK was lost | Divergent boards; no safe acceptance check | Reject |
| Suppress retry exhaustion only | The original regression survives but MOVE remains BUSY | Permanent game blockage | Reject alone |
| Correlated background withdrawal, yielding to continued play | Reuse CANCEL and the existing undo latch; no new wire bytes or Spectrum state | Late ACK before/after a confirmed next MOVE; shared DIRECT/MQTT corpus | Apply |
| Autonomous remote deadline | Reuse the current decision timer/frame scheduler; retries do not rearm it | Prompt expiry without CANCEL; 50/60 Hz and wrap tests | Apply |
| Compact the private scheduler, cancellation and notice helpers in Z80; reuse fastcall/callee arguments and existing wire strings | C-only candidate failed Next SP_GAP at 2965 B; final native helper vectors retain the C policy and error paths | Stack/register ABI, deadline wrap, replay and TX failure; 3218 CPU vectors and linked gate | Apply |
| Clear obsolete control bookkeeping at every committed MOVE | The one-operation guard admits only no control or background cancellation at commit; removes a redundant branch | Shared continuation/crossed-ACK transcripts; host and linked BOARD gates | Apply |
| New two-phase undo protocol | Requires capability negotiation and new verbs on all peers | Unnecessary compatibility and firmware cost | Defer |

The narrow DIRECT regression failed on `588a2a6` while PING still worked: the
session ended and the following MOVE could not commit. The candidate retained
correlation and accepted the next peer MOVE; that regression passed. Scope:
common reducers and timer coordinator; compact scheduler/control and board-input
busy gate; shared transcripts/observers; canonical contracts/manuals and handoff.
Cancellation retries remain in the background until a reply or a new committed
position proves they are obsolete. DRAW/RESET withdrawal may coexist with an
uncommitted local MOVE: a crossed acceptance discards that candidate; a confirmed
MOVE discards the withdrawal. Accepted RESET/DRAW replies are replayed to CANCEL,
just like accepted TAKEBACK, instead of falsely confirming withdrawal with NACK.

Final `make full-check PYTHON=/tmp/netchesszx-cancel-python/bin/python` passed
on macOS with z88dk revision `762578732c`: guards, host/ASM, both overlay ABI
manifests, unchanged size baselines, real-entry BOARD and live-interrupt IY.
Qt `make client-test` passed 18/18; `make client` installed the signed app.

| Linked measurement | Published §14 reference (Windows) | Final (macOS) | Constraint |
| --- | --- | --- | --- |
| Classic resident | 35739 B | 35751 B | <= 35753 B, 2 B free |
| Classic BSS / SP_GAP / CONTROL | 415 / 669 / 1935 B | 415 / 669 / 1935 B | Unchanged |
| Next resident | 32770 B | 32751 B | SP_GAP floor |
| Next BSS / SP_GAP / CONTROL | 513 / 3413 / 2143 B | 513 / 3432 / 2143 B | SP_GAP >= 3072 B |

The reference and final links are from different hosts; these are linked
budget comparisons, not an isolated patch saving. No firmware baseline was
built before editing. A same-flag app-object experiment against `588a2a6`
measured Classic code+rodata +5 B, unchanged BSS; it is not a linked-size claim.
The failed C candidate's Next SP_GAP 2965 -> final 3432 recovers 467 B of linked
margin. The native helper/C fallback pairs reuse the existing seven-byte stack
scheduler and persistent fields; no overlay-only code moved resident. Remote
MOVE validation retains withdrawal until a successful commit, including illegal
candidates. No limits, ABI baselines, configuration or save formats changed.

Stop after this successful gate. Next validation is physical Classic/Next at
50/60 Hz with lost cancellation and crossed acceptance, plus stack high-water.
SpectraNext firmware remains unbuilt; CPU vectors cover its timing branches only.

## Second aggressive shrink session (2026-10-13)

Base `93074df` (main after the shrink-agent micro edits), one z88dk
(revision `762578732`, macOS), measured commit against commit on the same host.
Pressure was Classic resident: 35714 B with BSS ending 66 B below the fixed IM2
vector at 0xFCFF.

| Target | Resident before -> after | Free below IM2 / SP gap | Overlays |
| --- | --- | --- | --- |
| Classic | 35714 -> 33995 (-1719) | 66 -> 1785 B below 0xFCFF; SP_GAP 669 | MQTT_CONNECT 1688 -> 1982, GUI_LOG 1950 -> 2035, INPUT_EDIT 1556 -> 1601 (moved code); others -4..-95 |
| Next | 32721 -> 31615 (-1106) | SP_GAP 3462 -> 4568 | moved code only grows MQTT_CONNECT (2133/4096) |

Acceptance: at least 30 B Classic per change, every overlay within its cap,
focused tests green; moved code is overlay-only code leaving resident.

| ID | Change | Classic | Decision |
| --- | --- | --- | --- |
| G1+G2 | COPT: shared `ld sp,ix; pop ix; ret` exit (JP) and HL-preserving IX entry helper for inline prologues | -120, overlays -146 | accepted |
| G4a | `spectrum_overlay_exec(_cached)` callee; dispatchers pop the packed id/entry word | -43 | accepted (entry-ABI guard updated) |
| A1 | Delete peer/transfer re-checks behind SESSION_PEER_GATE/TRANSFER_GATE/CAN_DISPATCH_CONTROL | -129 | accepted |
| A2 | One `session_busy()` leaf for the repeated busy test (RESET site untouched) | -59 | accepted |
| B4 | `parse_sid_end` shared by the MQTT H/J/side parsers (common code) | -50 | accepted |
| A4 | `session_confirm_prompt` packed fastcall for Y/N prompts | -33 | accepted |
| C1 | `draw_ikkle_text_at` as an adapter into `draw_ikkle_text_abs_y` | -219 | accepted |
| C2 | Timer strip repainted cell by cell instead of a region clear | -88 | accepted (transient repaint order differs) |
| C3 | Banner title in one scaled-text loop | -84 | accepted |
| C5 | Shared tmp_col address tail in text renderers | -40 | accepted |
| C4 | File and rank coordinates in one loop | -77 | accepted |
| B2 | Input-edit cell painters moved into INPUT_EDIT | -118 | accepted |
| B3 | ULA log scroll moved into GUI_LOG (Classic; Next keeps it resident) | -71 | accepted |
| B1 | MQTT broker-reply wait/stash moved into MQTT_CONNECT | -343 | accepted |
| G5 | `MAX_ALLOCS_PER_NODE` 50000 by default, 200000 for `make release-check` | -249, overlays -280 | accepted; full TAP build ~1.3x |
| G3 | Booleanization peephole (40 sites) | not built | deferred: flag liveness not provable with COPT text rules, ~80 B |
| G4b | Remaining callee/fastcall conversions | not built | rejected: 10-25 B each |

G5 sweep over G1+G2 (Classic resident / overlay total / full TAP build):
20000 -208 / -138 / 118 s; 50000 -229 / -266 / 119 s; 200000 -363 / -399 /
566 s, against about 90 s without the flag.

Baselines: ABI and size baselines were regenerated at 50000 on this host; the
ABI diff is only the moved symbols. Validation: focused host/ASM vectors per
change (new ikkle-text and scaled-text vectors), `make test`,
`make module-guards`, `make client-test` (18/18), one closing `make full-check`.
Not built: SpectraNext and the Next-UART diagnostic TAP (B1 moves code into its
MQTT_CONNECT). GUI_LOG has 13 B of Classic headroom left.

Emulator A/B (ZXESPEmu/ZEsarUX, Classic, `tools/emu_ply_trace.py` flow plus
screenshots, `93074df` against `226fd57`): Setup with name and time editing,
DIRECT host, game start and 16 plies typed in the editor complete on both; the
per-ply overlay loads and SD I/O match apart from where keepalives fall; both
`.RCV` slots (generation 17 ply 16, generation 16 ply 15) are byte-identical;
every screenshot matches except the wall-clock times in the move log and status
clock. C1 costs time: one `draw_ikkle_text_at` call takes 53912 T-states for
the 23-character timer line against 25333 before (notice text 41874 against
19818), about 8 ms more per timer repaint with identical pixels. Notice lifetime
counts loop iterations, so a notice can stay slightly longer when that repaint
spans a frame (seen as "Type move/cmd/chat" still up about 5 s after ENTER).

Still pending: MQTT connect and subscribe (B1; no emulator broker/peer), chat
scroll, the flipped board (vector only), Y/N prompts (A4), Next and SpectraNext
in an emulator, and hardware for timing/MMU/IFF. `make release-check` (200000) has not
been run; its overlay sizes after B1/B3 are unmeasured.

## Overlay headroom session (2026-10-13)

Base `07858d9`. Resident had 1785 B free below the Classic IM2 vector, so the
pressure moved to the Classic overlays near their 2048 B cap: GUI_LOG 2035,
SETUP 2033, ABOUT 2017. Measured on macOS with z88dk `762578732`,
`MAX_ALLOCS_PER_NODE=50000`; resident code is unchanged (only the overlay
atlas offsets move).

| Overlay | Change | Before -> after | Evidence |
| --- | --- | --- | --- |
| SETUP, TIME_CONFIG | Compact the shared `edit_buf` editor | SETUP 2033 -> 1965, TIME_CONFIG 1961 -> 1893 | new `edit_buf_vector` (old vs new over every valid state x 16 keys) |
| SETUP | Merge the GAME/LINK select paths; reuse nav-next on unchanged reselect | 1965 -> 1921 | `setup_visible_vector` cases 70-74 |
| SETUP | Drop the empty-text pre-check (each validator rejects empty text) | 1921 -> 1899 | cases 75-77 |
| GUI_LOG | Eight more message dictionary tokens, then bit-7 word ends | 2035 -> 1996, then 1643 -> 1607 | notify vector, all 51 messages |
| GUI_LOG | Chat word wrap as one ASM routine | 1996 -> 1643 | new `gui_log_chat_vector` (172 cases, passed on the old C first) |
| GUI_LOG | Move cells written by an ASM helper | 1607 -> 1545 | move-cell vector and a 99-operation old/new replay |
| ABOUT | Stats working data in overlay scratch (0x672B Classic/SpectraNext, 0x3C78 Next) | 2017 -> 1932 | `test_game_stats_render.py` with a 0xA5-filled scratch |
| ABOUT | Pixel copy stepped with resident `pixel_down_hl` | 1932 -> 1889 | new `run_about_render_vector` (every pixel and attribute) |
| ABOUT | Read failures folded into `about_read_input`; redundant handle reload | 1889 -> 1867 | 19 failure exits in the vector |

Totals: GUI_LOG -490 (503 B free), SETUP -134, ABOUT -150, TIME_CONFIG -68;
Classic overlays 36135 -> 35293. Next GUI_LOG 1964 -> 1474, SETUP 2033 -> 1899.
Not done: GUI_LOG-only allocation budget (-22, subsumed by the ASM rewrites),
`asset_filename` reuse (-13, needs a resident export), resident-cost
tradeoffs (message blob +751, `edit_buf` +167, ZX0 decoder +107).

Validation: `make module-guards`, `make test`, size baseline regenerated, one
closing `make full-check` (ABI manifests unchanged). Emulator A/B against
`07858d9` (Setup with name/time editing, DIRECT host, 16 plies, a wrapped
local chat): both `.RCV` slots byte-identical; screenshots differ only in
wall-clock times and the blink phase of the move marker. Not exercised in the
emulator: the About screen and the end-of-game statistics page (vectors only),
remote chat, SpectraNext and Next.

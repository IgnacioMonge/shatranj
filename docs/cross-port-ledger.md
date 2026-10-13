# Cross-Port Ledger (MirrorShift)

MirrorShift (`C:/dev/MirrorShift`, remote `mirrorshift`) is a separate product
built from a copy of this tree. The two repositories share no Git history, so
there is no upstream/downstream relationship: improvements travel in both
directions and each one is a deliberate decision, not an automatic merge.

This file records what has crossed, what was rejected, and why. Update it in
the same change that ports something.

## Sync Mechanism

`git cherry-pick` works across unrelated histories: it three-way merges by path,
and every shared file keeps the same path in both trees.

```
git fetch mirrorshift
git log mirrorshift/main --oneline                  # what is new there
git diff mirrorshift/main -- src/common/session/    # divergence in one area
git cherry-pick -x <sha>                            # apply
```

`-x` records the source SHA in the message, which is what makes this ledger
auditable. A commit already present here auto-merges to an empty diff, so a
speculative cherry-pick is a cheap way to test whether something is missing.

Measured divergence, 2026-08-30: 319 shared paths, 63 byte-identical,
164 diverged (29 at ten diff lines or fewer, 50 between eleven and fifty).
Normalising the product identifiers barely moves those numbers, so the
divergence is real product logic, not naming.

## Sync State

| Direction | Last reviewed | Head reviewed |
| --- | --- | --- |
| MirrorShift to here | 2026-08-30 | `mirrorshift/main` at `0c5b593` |
| Here to MirrorShift | 2026-08-30 | `main` at `1f91848` plus the working tree |

## Already Present Here

Reviewed against `mirrorshift/main` at `0c5b593`; no action needed.

| MirrorShift | Subject | Note |
| --- | --- | --- |
| `f6860c9` | Harden Spectranext DIRECT transport | Landed here in `1f91848`; `src/spectrum/overlay/direct_ovl.c` is identical. |
| `0c5b593` | Stop truncating the overlay recipe | Landed here in `1f91848` as `tools/build_overlays.sh`. |
| `1ce7d23` | Skip null memcpy in Direct ACK detail | Already guarded at `src/common/session/direct_session.c:318`. Cherry-pick auto-merges to an empty diff. |
| `813ae52` (session part) | Session end reasons | Done here independently in `bd70c8a` with `end_reason` plus `session_emit_end`; that shape is the better one. |
| `8045025` (helpers part) | Shared text helpers | `session_text_equal` and friends already exist and are used 82 times. |
| `aaecfcf` (board part) | `draw_board_clean` / `mark_mode` | Present in `asm/spectrum/screen.asm`; this one travelled the other way. |
| `1fff4f6`, `7833cef` (setup parts) | Endpoint editing and JOIN room preservation | Ported deliberately to the existing row-masked reducer: SELECT opens editable endpoints, valid JOIN+MQTT rooms survive GAME/LINK changes, and empty suffixes auto-enter the four-character editor. NetChessZX keeps its `NC` hexadecimal room format and chess-specific config ownership. |
| `1fff4f6` (SAVE transition) | SAVE success/failure state | Already equivalent in `session_setup_save` plus TIME_CONFIG init: success produces clean `EDIT/START` with START focused; failure keeps dirty `SAVE/START` with SAVE focused; both retain the complete visible menu. Covered here by the target TIME_CONFIG vector. |

## Rejected

| MirrorShift | Subject | Why not |
| --- | --- | --- |
| `138673e` (app.c part) | Accept DIRECT GAME START without peer ready | Breaks parity. The canonical reducer NACKs a DIRECT GAME START that arrives before the peer HELLO (`src/common/session/direct_session.c:1041`), so accepting it in the compact FSM alone is a target-specific semantic exception, which `.mex/patterns/change-protocol-session.md` forbids. It also treats a transport defect as a protocol rule: on the cartridge the HELLO goes missing because DIRECT is not drained between overlay calls, which is the gap tracked below. |
| `8045025` (SNTP part) | Remove the host SNTP copy | MirrorShift has no SNTP fallback to preserve. This product does: `src/spectrum/overlay/time_ovl.c` needs it while the Next RTC path stays unvalidated. |
| `813ae52` (`fat_timezone.h`) | Shared FAT timezone helper | The conversion in `src/spectrum/overlay/time_ovl.c:31` is the better one: it handles the 2100 non-leap year and the FAT year bounds, neither of which the MirrorShift helper covers. Its unit test is still worth taking; the helper is not. |
| `142c663` | `SPXN_XFS_SCRATCH_PRESERVE` for piece masks | MirrorShift stages live piece masks at `0x662b`, inside the XFS directory scratch. This tree does not: `asm/esxdos/overlay_loader.asm` has no `piece_sprite_addr`, and `SPXN_XFS_SCRATCH_PRESERVE_SIZE=0` in the `Makefile` is correct here. |
| `tests/tools/test_setup_flow.py`, `tools/check_sdcc_iy_contract.py` | Emulated setup and ABI probes | Not needed. MirrorShift puts its z88dk-ticks probes inside two large Python files (903 and 1104 lines). The same technique already lives here as nine standalone vectors under `tests/spectrum/*_vector.asm` (2926 lines) driven by `tests/tools/test_spectrum_asm_vectors.py`. Extend those instead of importing a second harness shape. |
| `af9dde0`, `b365dcd`, `d523871` | Checkers, reversi, disc rendering | Product-specific. |
| N28 audit | Rewrite the completed port topology as the current `main` checkout | The live SpectraNext schema-2 contract requires `repository.base`, `branch`, and `worktree`, requires base and branch to differ, and treats them as the immutable intake authority. Replacing both branches with `main` would invalidate the manifest; removing the keys would fail schema validation. |
| N06 audit | Change HOST peer-timeout lifecycle | The current host cleanup and guest wait asymmetry is normative in `docs/session-core-contract.md`; Qt rearms the reducer on the still-live broker link. No permanent replacement failure remains to fix. |
| N19 audit | Preserve arbitrary digit-leading move notation | Canonical SAN does not begin with a digit, while deployed legacy clock tokens do. Broadening the parser would add resident code for an unspecified wire form. |
| N20 audit | Change invalid/out-of-phase MOVE handling | Route and retained filters are already enforced, while zero/out-of-phase behavior differs across deployed DIRECT and MQTT flows. No replacement policy or interoperability failure is established, so changing replies or liveness would be speculative. |
| N38 audit | Add a nested-overlay runtime guard | No nested caller is known. The guard would spend resident bytes and require new MMU/IFF recovery semantics for a latent misuse. |

## Owed To MirrorShift

MirrorShift has `netchesszx` as a remote, so the same cherry-pick flow works
from there. Evidenced candidates, none sent yet.

1. **The DIRECT edit-line bounds vector**
   (`tests/spectrum/setup_menu_render_vector.asm` plus
   `assert_edit_line_within_buffer` in
   `tests/tools/test_spectrum_asm_vectors.py`). MirrorShift carries the same
   exact-fit buffer, `DEFS 30` at `entry_menu_config.asm:1231` with the
   15-character host clamp, and nothing guards it there.
2. **The timezone conversion** in `src/spectrum/overlay/time_ovl.c:31`.
   MirrorShift's `src/spectrum/transport/fat_timezone.h` misses the 2100
   non-leap year and the FAT year bounds. Its unit test
   (`tests/spectrum/test_fat_timezone.c`) is worth taking in exchange.
3. **The `end_reason` shape** from `bd70c8a`: one `session_emit_end` helper
   against MirrorShift's `direct_emit_end` that patches the previous action
   in place. Cosmetic; both work.
4. **The PORT auto-entry fix** in `asm/overlay/setup/entry_setup.asm`:
   call `su_room_editable` for every candidate row and return
   `_setup_port_text` for row 3. MirrorShift still checks only row 2 and its
   PORT branch leaves `HL` stale, so selecting DIRECT cannot auto-edit an empty
   port reliably there.
5. **MQTT PUBACK failure handling**, reviewed on 2026-09-07 against
   MirrorShift `c396a994`. Its `mqtt_wait_packet_into` still consumes the old
   packet length after stashing can abort and clear the stream; a 31-byte
   PUBLISH then wraps the length to 225 and overwrites the adjacent queue.
   Port the active-stream check before consumption from
   `src/spectrum/transport/net.c` and the PUBACK-send-failure regression in
   `tests/spectrum/test_mqtt_stream.c`. The other MQTT fix needs no port:
   MirrorShift already guards `mqtt_take_stream_packet` with `mqtt_discarding`
   and tests fragmented oversized packets. This review changed no MirrorShift
   files; its pending overlay/tool edits remain untouched.
6. **esxDOS raw handle zero**, reviewed on 2026-09-07 against MirrorShift
   `c396a994` plus its working tree. Classic/Next file and directory wrappers
   still store raw `A` on success and zero on failure; CONFIG, SAVELOAD, and
   FILEUI consequently reject a valid raw handle zero and skip its close.
   Port the `handle + 1` encoding and matching I/O decoding from
   `asm/esxdos/esx_fileio_spectalk.asm`, with the real-ASM handle regression in
   `tests/tools/test_spectrum_asm_vectors.py`. SpectraNext uses the external
   XFS adapter, which already encodes handles this way; no port needed there.

The same review found **MQTT Remaining Length above 255 already safe** in
MirrorShift: `mqtt_take_stream_packet` aborts the stream when the second
length byte exceeds one. The valid 300-byte body regression from
`tests/spectrum/test_mqtt_stream.c` can be ported without a runtime change.
This check made no MirrorShift edits.

## Overlay-Resident Placement, 2026-08-30

`spxf_replace_atomic` belongs in the CONFIG and SAVELOAD overlays, not in the
resident. Linking it resident costs 349 bytes and breaks the stack floor here:
the Spectranext SP gap is 588 bytes against a 512-byte floor, so this tree has
76 bytes of headroom and the helper needs 352. Measured, and then corrected, in
one sitting; recorded so nobody repeats it.

That placement is the whole reason the driver split exists. SpectraNext
`1506683 feat(driver): split atomic replace helper` landed 2026-08-28, the same
day MirrorShift adopted it in `021c09f`. A standalone `spxf_replace.asm` linked
with `spxn_rom.asm` pulls neither `spxf.c` nor its handle state, which is what
makes 349 bytes small enough to sit inside a 2048-byte overlay slot
(`driver.md:104`, `storage.md:219`).

MirrorShift wires it that way and pays nothing resident:

```text
Makefile:385            SPXN_ATOMIC_OBJ := $(BUILD_DIR)/spxf_replace_ovl.o
build_overlays.sh:102   ... saveload_ovl.o ${SPXN_ATOMIC_OBJ} ...
build_overlays.sh:126   ... config_ovl.o   ${SPXN_ATOMIC_OBJ} ...
```

One copy per slot. MirrorShift's Spectranext CONFIG sits at 2039/2048 *with*
the helper already inside, so the slot does hold it. This tree now does the
same. Two consumers means two copies; if a third overlay ever needs it, weigh
a resident copy against the recovered slot space then, not before.

Built and measured here 2026-08-30 with the overlay placement:

| | Baseline | Atomic replace, resident | Atomic replace, overlays |
| --- | --- | --- | --- |
| SP gap | 588 | 236 (floor breach) | **585** |
| CONFIG slot | | | 1842 / 2048 |
| SAVELOAD slot | | | 1584 / 2048 |

The three lost bytes are the cold-boot `netchesszx_hinted_rows` clear, not the
helper, which costs the resident nothing. `_spxf_replace_atomic` no longer
appears in `build/spectranext/SHATRANJ.map` at all. The 768-byte SP warning is
pre-existing: the baseline sat at 588, also under it.

## Applied From The 2026-09-06 Audit

- N04: the common reducer supports the real five-action MOVE collision and
  rejects insufficient action capacity before mutating state or workspace.
- N09: DIRECT liveness is credited only by recognized valid input; valid MACH
  remains a DIRECT credit and MQTT MACH remains informational only.
- N10: NACK RESET, DRAW, and GAME START matching requires a complete token.
- N15/N35: local CHAT is blocked by pending MOVE on Spectrum, and shared MQTT
  parity preserves the session end reason.
- N13/N17: changing clock source resynchronizes it, while numeric offsets stay
  local; save-slot timestamps now follow the live clock across offsets and days.
- N14: desktop slot scans retain the exact enumerated filename, so mixed-case
  files and duplicate slots remain loadable and erasable on case-sensitive FS.
- N37: configuration load distinguishes an absent file from real I/O failure
  while retaining defaults in both cases.
- N36: FILEUI scans to EOF and keeps only the newest timestamp per slot in a
  fixed 130-byte overlay-scratch table; duplicates cannot hide later slots.
- N22: overlay atlas output is replaced only when its bytes change, preserving
  timestamps for both packed and 8192-byte paged layouts.
- N32: AppImage documentation now states that release attachment is manual;
  the workflow itself only uploads a workflow artifact.
- N23/N24: Linux and macOS packages now carry project/third-party notices and
  license texts; the Lichess source metadata records the actual per-set terms.
- N33: macOS CI uses the same pinned Qt 6.10.3 installer as the other desktop
  workflows instead of the rolling Homebrew formula.
- MirrorShift `4a9fc6d`/`f9796ce`: shared session sends and dispatch epilogues,
  plus the compact `align_notice_text` from `aaecfcf`, reduce Classic resident
  35624 -> 35313 bytes; BSS stays 423 and SP gap grows 649 -> 960 bytes.

## Applied From MirrorShift, 2026-09-07

- Next RTC: `next_rtc_sync_time` no longer carries an `apply` argument across
  firmware ASM. It reads the selected timezone afterwards, retaining this
  product's validator, IX/IY/MMU1 preservation, and DRVAPI/GETDATE fallback.
  `run_next_rtc_vector` in `tests/tools/test_spectrum_asm_vectors.py` compiles
  the production C and executes the wrapper and validator in 12 Z80N cases,
  including numeric UTC+2 at noon and RTC during the first eight minutes of
  the day. ROM calls are simulated; real Next firmware remains untested.

The 2026-09-08 commit checkpoint refreshes the Classic size baseline from linked
artifacts: resident 35313 -> 35284 bytes, BSS 423 -> 421, SP gap 960 -> 991.
The accepted overlay costs since the old baseline are CONTROL +49 for reply
validation, DIRECT +12 for bounded UART draining, SAVELOAD +79 for exact file
length validation and handle encoding, and FILEUI +2 net after that encoding.
CONFIG remains 2037/2048; the total overlay atlas grows 28026 -> 28166 bytes.
The strict growth check stays enabled against this measured reference.

## Paging Audit Applied, 2026-09-08

Compared NetChessZX `14c32be`, MirrorShift `aaa57ee`, and the SpectraNext
driver at `f62e643`. Both consumers started clean. The driver is unchanged.

Next and SpectraNext share the non-nested 4 KiB executable-overlay model.
Their physical mapping is different: Next maps an 8 KiB MMU page at `0x6000`,
with the upper half mirroring immutable resident code at `0x7000`; writable
resident state remains above `0x8000`. SpectraNext maps cartridge Page B at
`0x2000` and must remove the cartridge to expose the Spectrum ROM during a
frame wait. Their interrupt contracts also remain distinct: NetChessZX's
dispatcher returns with EI; MirrorShift's cartridge dispatcher restores IFF.

| Finding | NetChessZX | MirrorShift cross-check |
| --- | --- | --- |
| Preload leaves the last overlay in Page B | Reproduced displaced page `0x49` becoming `0xD4`; matched PUSHPAGEB/POPPAGEB now restores it | Already paired |
| Failed XFS close loses ownership | Retain the open flag on carry so cleanup can retry | Already retained |
| Nested dispatch can mutate loader state | Independent active guard in both loaders, including cached entry and cartridge frame callbacks; reuse boot-only bytes | Guards already present |
| Raw ROM helpers have no public IX/IY guarantee | Preserve IX/IY across public execution and frame waits; protect IY during preload ROM windows; install Spectrum IY before HALT | IY already protected; added IX preservation |
| Dead cartridge code | Omit unreachable file-backed load/error blocks; remove duplicate RTC-probe bypass because TIME_CONFIG already returns unavailable | Both omissions already present |
| Fixed Next MMU setters and redundant saves | Native NEXTREG setters, remove copy wrapper and compressed-bank AF save: -20 bytes | Same changes, -20 bytes |
| NEX generation bypasses stack floor | Generator rejects gaps below 3072 bytes | Same guard; also wire existing Next layout tests into `make test` |

The nested-call guards close a latent invariant gap: the current application
call closure does not nest overlays. IX/IY protection hardens the public ABI;
the inspected current ROM versions preserve these registers. The executable
ROM models deliberately clobber them to verify caller and frame restoration.
Preload's sole production caller already owns IX preservation.

The [official Z80N instruction reference](https://wiki.specnext.dev/Extended_Z80_instruction_set)
defines `NEXTREG reg,A` (`ED 92`). The linked saving is four setters (-8),
the obsolete copy wrapper (-10), and redundant AF saves (-2). BC protection
around the mirror LDIR remains necessary. No cycle or hardware-speed claim.

| Linked Next measure | NetChessZX before -> after | MirrorShift before -> after |
| --- | --- | --- |
| Resident bytes | 32041 -> 32053 (+12) | 30985 -> 30965 (-20) |
| BSS bytes | 522 -> 522 | 487 -> 487 |
| SP gap bytes | 4133 -> 4121 | 5223 -> 5243 |

NetChessZX's new Next guard costs 32 bytes, offset by the 20-byte saving.
Overlay sizes and ABI entry tables are unchanged. Classic resident/BSS/SP gap
remain 35284/421/991 here and 33522/407/2767 in MirrorShift. The selected
Classic loader instructions match HEAD in both; this tree's Classic CODE
also retains its previous SHA256.

For SpectraNext, a paired assembly using the same latest harness and flags
measures 2128 -> 2061 bytes: **67 fewer loader bytes**, including all new
protections. This is an isolated loader delta, not a whole-image comparison
against the older cartridge artifacts. The lower backend grows 486 -> 510
bytes here and 502 -> 512 in MirrorShift, where IX protection costs 10 bytes.
Final occupied ranges are `0x6E00..0x6FFD` and `0x6E00..0x6FFF`, respectively;
the existing linked guard enforces the exclusive `0x7000` ceiling. Final
cartridge SP gaps are 1221 and 2151 bytes. No BSS was added by this work.

Validation completed:

- Both production-ASM Next vectors pass packed arguments/context, full HL
  return, bounds, nested rejection, caller IX/IY/SP and MMU register/IFF
  preservation. Generator boundary cases reject 3071 and accept 3072 bytes.
- Both cartridge vectors pass preload/rollback, fingerprint, short reads,
  page restoration, entry bounds, nested frame callbacks, IX/IY/SP/IFF and
  failed-close retry. These use modeled ROM vectors, not a hardware boot.
- MirrorShift `make full-check` passes. The added test wiring was checked
  separately with `tests/tools/test_next_overlay_pages.py`; the final
  cartridge change passes `make tap-spectranext SPXN_DIR=C:/dev/SpectraNext/driver`.
- NetChessZX's one `make full-check` completed host/module tests and Next ABI.
  A Makefile test-dependency edit during its Classic atlas pass invalidated
  the fingerprint. With inputs frozen, only the remaining effects were run:
  `make size-check SIZE_CHECK_FLAGS=--fail-on-growth`, the canonical Classic
  ABI comparison, and the Next size report with `--min-sp-gap 3072`; all pass.
  Final cartridge changes pass `make spectranext-size-report
  SPXN_DIR=C:/dev/SpectraNext/driver`. Subsequent IX edits affect only the
  cartridge-selected instructions, so green Classic/Next builds were reused.

Commands used `PYTHON=C:/Progra~1/Python311/python.exe`. Exact reports, hashes,
logs and the pre-edit option ledger are under each tree's ignored
`build/paging-audit/`; the combined evidence is `summary.json` here.
No physical Next or SpectraNext acceptance is claimed. Physical-pager
unification, larger executable overlays and weakened interrupt protection
were rejected because they change the established layout/ABI contracts.

Workflow: root integrated and verified; two investigator spawns and one
verifier spawn plus one follow-up review. Workers requested gpt-5.6-sol/high;
the actual model override was not independently confirmed. The final
independent review found no remaining blocker.

## Pending

Ranked. Nothing here is started.

1. **Spectranext size gate**. `z80opt.toml` has no `[targets.spectranext]`
   section, so the cartridge resident grows without a tripwire. MirrorShift
   pins 35822 resident and 513 SP gap. The number here has to come from a
   measured Spectranext build; do not copy MirrorShift's.
2. **Spectranext in the repository gates**. `full-check` is
   `module-guards test abi-check size-check nex-size-report`, so
   `spectranext-port-build` never runs. No workflow links the cartridge driver
   either; MirrorShift's `ci.yml` checks out `IgnacioMonge/SpectraNext` and
   runs `make tap-spectranext`.
3. **DIRECT background drain on the cartridge** (MirrorShift `021c09f`,
   `src/spectrum/transport/spectranext_direct_stage.c`).
   `src/spectrum/transport/net.c:133` states the current decision: DIRECT is
   drained only by its overlay, and the cartridge is expected to buffer while
   resident code runs. MirrorShift stages one recv in low RAM instead. Needs
   hardware evidence before porting; it is the suspected root cause of the
   dropped HELLO behind the rejected `138673e`.
## Known Divergence, Not Scheduled

The canonical reducer answers a DIRECT GAME START received before the peer
HELLO with `NACK GAME START BAD` (`src/common/session/direct_session.c:1041`).
The compact FSM stays silent and shows the wait-for-opponent notice
(`src/spectrum/app/app.c:3202`). The same asymmetry exists for a HOST that
receives GAME START: the reducer sends `NACK GAME START HOST`, the FSM returns
handled.

Neither path is reachable over a healthy TCP stream, because HELLO always
precedes GAME START on the same connection, which is why no transcript covers
either one. They become reachable exactly when the transport drops bytes, which
is pending item 3. Aligning the FSM on the reducer costs resident bytes on a
target that cannot be size-checked without a Spectrum build, so it stays
recorded rather than fixed. Do not resolve it by relaxing the reducer.

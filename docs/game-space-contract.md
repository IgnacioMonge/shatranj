# Logical game spaces

## Scope

Developed in the isolated `codex/game-space` worktree, based on
`d660e807286cb3f6478d843aa57b612947f77740`. The pre-edit
`ux-interaction-test` and `layering-check` passed; no pre-edit firmware build
was performed. Main integration was authorized after the user reported
successful manual ZX and Next tests.

Logical identity and cursor navigation no longer require an 8x8 rectangle.
Shatranj remains a chess consumer with specialized rules, presentation,
protocol and saves. A standalone PC/Spectrum consumer proves reuse with a
12x10 rectangle, a hexagonal region with a hole and single-cell activation.
This is compile-selected composition, not a runtime plugin framework.

## Implementation decisions

| Option | Cost and risk | Decision / acceptance |
| --- | --- | --- |
| Logical IDs plus immutable rectangle helper | Bounded 16-bit capacity; arithmetic cost on Z80 | Implement; bounds/overflow tests, link only where consumed |
| Separate private cursor and chess selection | Extra calls may affect resident size and stack | Implement; 1024 navigation cases, interaction checks, linked ABI/size gate |
| Compile-selected navigation | One direct call, no persistent dispatch table | Implement; actual chess and demo bindings |
| Same-unit chess repaint extraction | Keeps private views and function placement | Implement; unchanged bodies and ABI checks |
| Native PC/Spectrum demo | Small consumer, full redraw limited to 120 cells | Implement; events, views, connectivity and Z80 rendering checks |
| Universal board state, runtime registry or generalized renderer | Duplicate ownership, dispatch and RAM with no current consumer | Reject; each game supplies its own model and presentation |
| New-game protocol, saves and hardware delivery | Separate product/security and physical validation work | Defer; no such behavior is claimed here |

## Ownership and contracts

| Owner | Responsibility |
| --- | --- |
| `common/space` | Logical IDs and immutable rectangular adjacency |
| `ui/cell_cursor.c` | One private cursor; home, read and move operations |
| Selected `space_navigation.h` implementation | Geometry and visual key/orientation mapping |
| `ui/board_interaction.c` | Shatranj selection, origin/destination, confirm/cancel and intentions |
| `ui/chess_move_view.inc` | Chess move repaint/animation consequences inside the existing GUI unit |
| Game/model | State, legal actions and player-visible facts |
| Application | Coordinate owners through explicit operations |
| Session / transport / platform | Their existing independent contracts |

Connectivity is not legality. A visual direction is not a universal topological
edge: rectangle directions belong to `rect.h`, not the identity contract.
The demo defines its six hexagonal connections locally; adding that consumer
required no hexagon framework or changes to the cursor owner.

- `space_cell_t` is an internal 16-bit identity. `SPACE_CELL_NONE` is reserved,
  allowing 65535 cells (IDs 0 through 65534); it defines no wire/save format.
- `space_rect_t` contains caller-owned dimensions. Count rejects null, empty
  and oversized rectangles, widening multiplication before evaluation on Z80.
  IDs are `row * columns + column`; neighbor returns NONE for invalid input
  and outer edges. There is no wrapping, allocation, retained pointer or cache.
- The cursor treats IDs as opaque. Its caller supplies a valid home; move
  invokes the selected presentation operation and ignores a NONE result.
  Storage is private, with no mutable view, callback table or application bus.
- Shatranj's navigation binding preserves its packed row/column IDs, both
  orientations and all old directional aliases. Its public interaction ABI,
  admission, redraw order and selection behavior remain intact. Variable-width
  rectangle arithmetic is not linked into its Spectrum hot path.
- Cursor and chess selection each own a private 16-bit value, retaining four
  bytes total. Selection cannot be accessed by the independent cursor/model.
- Chess-specific repaint bodies are composed at their original location in
  `gui.c`. They retain the private board view, optimized render kernels and
  assembly placement; no extra wrapper or board snapshot was introduced.
- Existing dependency gates reject game/platform imports in `common/space`,
  undeclared dependencies in the cursor and external access to private UX state.
  Negative tests cover these rules. Lexical guards supplement code review and
  C-private storage; they are not a general proof against arbitrary source code.

Desktop Shatranj retains its existing Qt widget ownership. The demonstration's
Qt executable links the same C cursor used by Spectrum Shatranj, with its own
geometry implementation. Each program uses one cursor; there is no new global
state shared between processes or replacement of Qt's per-widget state.

## Demonstrator

`examples/space_demo/` contains a small model and two native presentations:
Qt polygons and Spectrum bitmap drawing. Both consume `demo_view(cell, player)`;
the reveal array is private. Each player initially sees alternate cells.
Activation reveals one cell for that player. This verifies filtered views and
single-cell interaction, not a new game's rules or network privacy.

The rectangle has 120 cells. The hexagon has 36 valid positions in a 7x7 axial
bounding region, with the center absent. Invalid positions and the hole cannot
be entered. Shapes fit the Spectrum 256x192 canvas. The Spectrum demo redraws
at most 120 cells per action; incremental painting is left to a game that needs
frequent updates. It does not claim animation-rate rendering at 3.5 MHz.

Build from the worktree root with the repository toolchain:

```text
make space-test
make client-test
make space-demo-check
```

On this Windows host, `client-test` uses the existing Qt installation through
`QT_ROOT_DIR=C:/Qt/6.11.1/msvc2022_64`. Make's Python recipes can use the verified
short path `PYTHON=C:/Progra~1/Python311/python.exe` when needed. The desktop
entry point remains `make client-test`; do not bypass its environment wrapper.

- PC executable: `build/pc-build/Release/shatranj-space-demo.exe`.
- Spectrum artifact: `build/space-demo/SPACEDEMO.tap` (`space-demo-tap` builds it
  without the instruction-execution check).
- QAOP moves; E/Z supplies the other two hexagonal directions; Space reveals;
  V switches player view; 1/2 resets the rectangle/hexagon. PC also accepts
  arrow keys, Enter and left-click activation.
- Desktop self-test screenshots: `build/pc-build/space-preview/`.

The PC executable needs Qt's runtime on `PATH`, as configured for the tests.
To open it on this Windows host from the worktree root:

```powershell
$env:PATH = 'C:/Qt/6.11.1/msvc2022_64/bin;' + $env:PATH
& ./build/pc-build/Release/shatranj-space-demo.exe
```

The demo links only its model, rectangle helper and productive cursor, plus
native UI/runtime libraries. The Z80 check rejects chess/session/network
symbols in its linked map. No product protocol or save migration is involved.

## Verification

- `make space-test`: capacity, overflow, all rectangle edges/inverses, wide IDs,
  cursor invalid-result handling, both demo geometries, reachability, the hole,
  pixel bounds, private reveals and reset.
- `make ux-interaction-test`: 1024 cursor comparisons against an independent
  rectangle reference (64 cells, two orientations, eight key aliases), plus
  existing selection, admission, rejection, cancellation and editor scenarios.
- `make client-test`: 14/14 desktop tests passed, including real Qt keyboard and
  pointer events, hole rejection and screenshots of both partial views. Windows
  uses its native Qt backend for the hidden demo widget so font rendering is
  representative; other platforms retain the offscreen test backend.
- `make full-check`: combined host/guard, Classic/Next firmware, ABI and memory
  closure; the final measured result is recorded below. No baseline is raised.
- `make space-demo-check`: compiled Z80 initialization and renderer execute in
  `z88dk-ticks`. All 120/36 outlines, the empty hole, return address and RAM
  canaries pass. Rectangle: 2,187,805 ticks; hexagon: 1,181,346 ticks. These are
  synthetic execution counts, not measured hardware frame times.

The Z80 demo test starts with a zero-filled ROM region: it verifies geometry
and memory behavior, not real ROM glyphs, keyboard scanning or cartridge I/O.
PC screenshots are visually reviewed. No new physical Spectrum/Next or
SpectraNext run has been performed; existing hardware results belong to their
original artifacts and are not reused as proof for this worktree.

A full target build exposed a z88dk `-SO3` peephole defect: repeated byte casts
of the global packed selection in a redraw macro produced invalid `ld hl,a`.
An isolated reproduction failed with and without project peephole rules and
compiled at `-SO0`. Capturing the cell once in a small fastcall redraw helper
compiles at the required `-SO3` with the project rules. The build flags and
optimizer rules remain unchanged; the diagnostic worktree was removed.

### Linked measurements

The first production-navigation checkpoint passed `full-check` with Classic
resident/BSS/SP gap 35230/420/1046 bytes and Next 32093/521/4082 bytes. After
the independent cursor/selection extraction and compiler workaround, the
final `build/game-actions-checkpoint/space-verified-full-check.log` records (`full-check`, exit 0):

| Target | Resident | BSS | SP gap | Resident change from navigation checkpoint |
| --- | ---: | ---: | ---: | ---: |
| Classic | 35078 | 420 | 1198 | -152 |
| Next NEX | 31941 | 521 | 4234 | -152 |

Both ABI manifests match their unchanged baselines. Classic is 206 resident
bytes below its canonical size baseline (35284), which includes changes older
than this worktree; that is not a claimed task-only saving. No exact pre-edit
firmware comparison or dynamic stack high-water measurement was performed.
The SP gap is linked headroom, not a measured maximum stack usage.
The standalone Spectrum demo TAP is 3159 bytes.
The final gate also includes the corrected build dependencies and single
interaction source list. Its SpectraNext configuration was checked by querying
Make's source list only; no SpectraNext firmware build or hardware claim is made.
The publication close-out `build/game-actions-checkpoint/space-main-closeout.log` also passed
`full-check` with the same sizes and ABI after the final source formatting fix.

### Development launchers

At the geometry checkpoint, `zxespemu-launchers.json` identified the candidate as
`1.2.1-dev.space.1`. Classic and Next use the registered checkout through
the existing NetChessZX runtime integration. SpectraNext and its installer
use the in-repository manifest `./packaging/spectranext/port.json`; their
previous artifacts remain separate from this work's Classic/Next evidence.
ZXESPEmu owns shortcut replacement and duplicate cleanup.

### Manual acceptance

The user tested the rebuilt ZX and Next candidates through the updated
launcher workflow, reported no apparent behavior difference, and subsequently
confirmed successful operation after receiving the navigation, selection,
repaint, restoration and network checklist. This is user-reported manual
acceptance; no individual checklist transcript or additional physical-hardware
result is inferred. Historical SHA-256 identities recorded in
`build/game-actions-checkpoint/space-artifacts-manifest.json` for that candidate
(not the later game-action artifacts):

| Artifact | SHA-256 |
| --- | --- |
| Classic TAP | `38c6215ab3bef1384d2d8bc5e4718275dcf086e56e268556f8d22c3112f1df78` |
| Classic OVL | `1dc1c81effffd857ee26f2ac5d7b6ad6d73aec8c25d14155aba55ce16452b0b3` |
| Classic DAT | `d89830ddfb6f64cf86edce52e9e2e14310c6e056cc8d088c337ab0441d5c3779` |
| Next NEX | `4dfc79dcf9742d164652c1add17fa3fa9d762953d450a685ac611dbd4125c850` |

The recorded OVL hash above is present in the original manifest, but the OVL
currently retained in `game-space/release/` hashes to
`17609cdb9d5955a7b4e9a5b8475111f816c282eefe7bc73b1d2914656462c845`.
The originally recorded OVL cannot currently be recovered from that file.
Do not substitute the retained file's identity for the historical acceptance.

## Recovery and remaining product work

### Game actions (phases 0–2)

The action boundary reuses existing mutation points, with compile-selected
implementations. `common/chess/action.h` owns the three-byte chess value
(`from`, `to`, lowercase promotion or zero); it is not a serialized structure.
`legal.c` exposes coordinate-token decode/encode and checked
`netchesszx_rules_apply_action`. Decode normalizes uppercase promotion; typed
actions must already use the canonical lowercase value. Shape, promotion and
legality are checked before any mutation. Failed codecs leave outputs intact.

Desktop local and remote DIRECT/MQTT deliveries already converge through
`MainWindow::applyMoveToBoard` → `netchesszx_rules_play`; that compatibility
entry point now decodes and calls the typed operation. History remains in
`finishAppliedMove`, after acceptance. Session actions are delivery/effect
records, distinct from game actions; ply, colour, retry and ACK policy stay put.

Spectrum local input (`SESSION_LOCAL_MOVE_REQUEST`) and incoming MOVE
(`SESSION_APP_MOVE_IS_LEGAL`) retain admission through the board/rules owner.
On confirmation, `apply_pending_local_move` and `SESSION_APP_APPLY_REMOTE_MOVE`
both use `spectrum_board_apply_trusted_move_with_undo` → `board_apply_trusted`.
Its existing parsed-coordinate/borrowed-text BOARD ABI is retained. The existing
`board_apply_trusted_ovl` entry converts directly to the same chess action and
calls the single private `board_apply_action` mutation operation; the host
adapter is `board_apply_parsed_move`. This is an
**accepted-action** operation: coordinates and legality were checked by the
board owner for the current position; promotion/piece consistency is checked
again before board or undo writes. It is not an unchecked external input API.
The host board tests compile that same implementation. No new resident rules
implementation or extra RULES/BOARD dispatch is introduced.

The action is stack-local to the synchronous BOARD call. The adapter reads the
undo pointer only when applying, avoiding a live temporary during decoding.
For nonzero promotion bytes it folds ASCII bit 5; the action owner then accepts
only q/r/b/n. All 256 suffix bytes are checked against the accepted alphabet.
No action pointer
escapes or survives overlay replacement; resident buffers, context offsets,
entry symbols, bank switching and undo layout are unchanged. Decoding remains
target-specific where the Spectrum ASM parser and existing overlay context
already provide coordinates. Transport does not copy a C action as bytes.

RESTORE/load still restore validated snapshots, and TAKEBACK still restores
the existing rules snapshot (desktop) or compact undo (Spectrum). Neither is
reclassified as a move or replay journal. Protocol framing, notation, NCZS,
save wire version 1, and the 45-byte/60-character restore encoding are unchanged.

The demonstrator's `demo_action_t` contains only `space_cell_t cell`.
Both presentations' existing `demo_activate` adapter calls
`demo_apply_action(action, player)`; the model checks cell and participant
before changing private reveal state. No invented origin, shared action union,
session rule, transport codec or second engine is needed. Reuse demonstrated
here is the same ownership boundary alongside the existing shared cursor,
not a universal game engine or cross-game network protocol.

| Option | Cost / risk | Decision and verification |
| --- | --- | --- |
| Expose the existing parsed chess value and checked mutation | No new state; host code grows by explicit codec/API | Apply; `action-test` compares every cell, side, castle and EP |
| Adapt directly inside the existing BOARD entry | Three-byte automatic value; keep undo lifetime short | Apply; same trusted admission, special-move/undo tests; focused Classic link 1733 → 1728 bytes |
| Replace resident/session buffers or BOARD context ABI | Extra resident bytes, banking/compatibility work without a consumer | Reject |
| Give the demo its own single-cell value | Small synchronous adapter, no network dependency | Apply; direct/activation equivalence and invalid cell/player checks |
| Generic actor, hidden state, persistent journal | No consumer in this slice | Defer |

Baseline: clean `main` at `ff24b27`; initial rules, board and `space-test`
passed. Implementation was verified in branch `codex/game-actions`, worktree
`C:/dev/NetChessZX-worktrees/game-actions`, then integrated into `main`.
No pre-edit firmware rebuild. Existing original-checkout `size_report.json` and
maps date from September 11 and disagree with the integrated September 12
artifact, so they are not a task baseline. The pre-action Classic TAP matched
the historical candidate above; the relinked action TAP/OVL/NEX have different
identities. The pre-action recorded linked reference
is Classic 35078/420/1198 and Next 31941/521/4234 (resident/BSS/SP gap);
the initial demo TAP reference is 3159 bytes. These are prior recorded
measurements, not a fresh paired build. The focused BOARD check uses a copied
map/config from the unchanged `codex/game-space` checkpoint at `ff24b27`
(September 12, 11:20), whose resident inputs are unchanged by this slice.
The normal `overlay-size` stamp check passes; no timestamp or baseline is raised.
Recovery is the scoped diff against
`ff24b27`; no user files were dirty at the initial checkpoint.

`make action-test` adds direct-versus-MOVE semantic equivalence, both colours'
short/long castling, en passant, all four promotions, captures, checkmate,
Spectrum undo/reapply and atomic rejection. `make space-test` also compares
both player views after direct versus presentation-adapter activation and
rejects null actions, invalid cells, the hole and invalid players. Existing
protocol, save and session tests remain the compatibility judges.

The isolated closing `make full-check PYTHON=C:/Progra~1/Python311/python.exe`
completed with exit 0 (`build/game-actions-checkpoint/action-full-check.log`): host/action/space tests,
protocol/save compatibility, shared DIRECT/MQTT transcripts, module guards,
both overlay ABI manifests and Classic/Next size limits pass. The 77 MQTT
transcripts pass for Classic/Next at 50/60 Hz with their existing judge scope.
No ABI or size baseline was changed.

| Target / metric | Initial checkpoint | Final | Task delta |
| --- | ---: | ---: | ---: |
| Classic resident | 35078 | 35078 | 0 |
| Classic BSS / SP gap | 420 / 1198 | 420 / 1198 | 0 / 0 |
| Next resident | 31941 | 31941 | 0 |
| Next BSS / SP gap | 521 / 4234 | 521 / 4234 | 0 / 0 |
| BOARD code, either target | 1733 | 1728 | -5 |
| Classic OVL file | 28118 | 28113 | -5 |
| Demo TAP | 3159 | 3210 | +51 |
| Demo BSS | 388 | 388 | 0 |

The Next OVL file remains 139264 bytes because its pages are fixed-size.
Classic BOARD has 320 bytes free of 2048; Next BOARD uses 1728 of its 4096-byte
executable budget. The older canonical baseline's 206-byte resident improvement
is not attributed to this task. SP gap is linked headroom, not measured maximum
stack usage; no dynamic stack high-water or action cycle count is claimed.

The original Qt log is retained as
`build/game-actions-checkpoint/action-client-ctest.log` (14/14). The original
demo TAP/map/code are in `build/game-actions-checkpoint/space-demo/`.
The session reported a successful Z80 rendering run, but did not retain its
console log alongside those three files. The earlier 2187805/1181346 tick
figures are not used as independently reproducible proof of the action build.
The audit follow-up below replaces this evidence gap with fresh executions.
The user's successful manual reports followed installation of the final Game
Actions launchers; no exhaustive scenario matrix or physical-hardware claim
is inferred from those reports.

Changed files: `src/common/chess/action.h`, `src/common/chess/legal.h`,
`src/common/chess/legal.c`, `src/spectrum/board/board_apply_impl.h`,
`src/spectrum/overlay/board_apply_ovl.c`, `examples/space_demo/demo.h`,
`examples/space_demo/demo.c`, `tests/rules/test_actions.c`,
`tests/common/test_space_demo.c`, `Makefile`, `CHANGELOG.md`, and this contract.
Phases 1–2 are complete within the stated software-validation limits.

The implementation is reviewable against the recorded checkpoint. Recovery
should revert its source, tests, build, guard and documentation changes
together, preserving unrelated changes. Generated evidence lives under
`build/`; distributable artifacts live under `release/`.

A future game supplies its model, geometry/navigation binding, presentation
and actions. Its network messages, persistence and hidden-information security
need their own explicit contracts. Hardware acceptance must use freshly built
candidate artifacts; it is separate from the completed host and target software
checks.

The task worktree was retired after integration. Its generated build evidence
and Qt test executable are retained under `build/game-actions-checkpoint/` in
`C:/dev/NetChessZX`.

### Audit follow-up (2026-09-12)

No production C/ASM, ABI, memory placement or published format changed in this
follow-up. The two text adapters remain, but the production adapter now has a
direct executable regression: `test_board_action_z80.py` loads the linked
resident image and BOARD overlay, obtains the apply address from the real
overlay entry table, and supplies the unchanged eight-byte context with real
16-bit pointers. It runs 265 cases per target: all 256 suffixes, eight black
promotion suffixes and trailing-text rejection. It compares all board cells,
the rules-board representation, side/castle/EP, undo bytes, unchanged context
and borrowed text, and balanced SP. It does not invoke the host adapter.
`full-check` runs this test after linking both targets, preventing future
production-only decoder regressions from escaping the closing gate.

The demo's Z80 harness now calls both `demo_activate` and `demo_apply_action`,
checks hidden-to-revealed view changes, both participant bits, null/invalid
actions and invalid participants, then compares all 120 reveal bytes before
checking the renderer and RAM guards. Fresh `make space-demo-check` passed for
120/36 cells with 2195178/1201999 ticks for these complete test scenarios (not
isolated action cycle counts). Its TAP is byte-identical to the retained
3210-byte action demo. Fresh `make client-test` on the main checkout passed
14/14, including `pc-space-demo`, with tool-reported exit 0.

| Decision | Cost / verification |
| --- | --- |
| Execute the production BOARD decoder in Z80 | Test tooling only; zero product bytes; Classic and Next pass |
| Extract another decoder call into BOARD | Not needed after direct coverage; retain the measured 1728-byte overlay |
| Add bounds checks to the private accepted-action core | No reachable unchecked producer; retain existing admission and documented precondition |
| Initialize selection in nonzero data | Navigation is gated until `cursor_home`; no reachable reported startup failure, retain BSS |
| Repeat Classic/Next linking | No production inputs changed; reuse coherent checkpoint images/maps and verify published identities |
| Consolidate launchers | Canonical owner `netchess`, version `1.2.1-dev.actions.3+g7186f25`; temporary owner removed and unregistered |

The historical geometry logs and manifest were copied without modification
into `build/game-actions-checkpoint/` and verified by hash. The earlier partial
`build/action-full-check.log` is not closing evidence. Current audit logs are
`build/audit-board-classic.log`, `build/audit-board-next.log`,
`build/audit-space-demo.log` and `build/audit-client-ctest.log`.
`build/audit-evidence.json` records artifact/source identities and commands.
Generated local evidence is ignored by Git; the identities below remain in the
versioned contract. Root build maps may still be older; only the complete
checkpoint image/map/OVL set was used for BOARD execution.

| Current artifact | SHA-256 |
| --- | --- |
| `release/SHATRANJ.tap` | `0f68a184d9d525b4dc36fc761fc2a6029c47620ddc9b45f54d31f10607e152a9` |
| `release/SHATRANJ.OVL` | `15293af05fd0c21d365e459073f09e6cfca77d36e94a53a09f4468708eab1067` |
| `release/SHATRANJ.DAT` | `d89830ddfb6f64cf86edce52e9e2e14310c6e056cc8d088c337ab0441d5c3779` |
| `release/Next/SHATRANJ.nex` | `7bf10b6a4476c98f312a1667866aeaf126843f9b0f26e51272524c87c95eb112` |
| `build/space-demo/SPACEDEMO.tap` | `bc14e73aa21fdd18f66a5f5409709d8d0ce05a38cf3dc540b484a9967fb7dc25` |

Canonical Classic/Next desktop shortcuts now identify these action artifacts;
the redundant Game Actions descriptor and its two shortcuts are retired.
SpectraNext entries share the descriptor version, but still launch their
separate earlier artifacts; no SpectraNext firmware was rebuilt or validated.
The Qt shortcut uses the freshly tested executable. Historical OVL recovery,
physical hardware, interrupt/bank-switch execution, maximum dynamic stack use
and an exhaustive manual scenario transcript are not claimed by this follow-up.

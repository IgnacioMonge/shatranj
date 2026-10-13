# UX isolation implementation

Scope: Qt and the shared Classic, Next and SpectraNext sources. Preserve input,
presentation, protocol, saves, placement and overlay ABI. Starting checkpoint:
`33b2cc177de574132fa88fe58eb683ea2cfef36d`.

## Cuts and acceptance

1. Define ownership: application owns commands, authoritative history and domain
   admission; UX owns focus, selection, editing, scrolling and composition;
   views consume coherent borrowed data; kernels render; platform owns devices.
2. Extract concrete Qt board/input widgets and Spectrum interaction operations.
   Keep synchronous application intentions; no bus, queue or generic framework.
3. Separate ASM hint coordination from legality and rendering in the existing
   assembly units. Keep physical input separate from presentation ticks.
4. Test productive interaction code and forbidden dependencies, then run one
   desktop close-out and one combined Spectrum host/ABI/size close-out. Record
   target-specific evidence and measured costs before declaring completion.

Board interaction now uses a separately compiled resident owner with private
storage and declared application fact/intention functions. Legal hints originate in application/domain coordination;
renderers never calculate legality. Command recall is UX state, unlike semantic
game history. Borrowed views are valid only during their synchronous use and
must not outlive the backing buffer or overlay.

## Contract scenarios

The scenarios are shared expectations, not a portable UI framework. Spectrum
links production board interaction independently and exercises the existing editor code; Qt uses its concrete
widgets. Existing FILEUI, GUI_LOG and desktop restore tests retain their coverage.

| Scenario | Required observation |
| --- | --- |
| Navigation / orientation | Normalized direction selects the corresponding visual square; no board mutation |
| Select / confirm / reject | Selection is UX-owned; move intention is admitted by application; rejection preserves position |
| Cancel | Selection or editing ends without applying a domain command |
| Editing / recall / scroll | Cursor, bounded recall and visible position change without altering game history |
| Visibility / restoration | Projection redraws from current position; text editing survives redraw; stale selection is cleared by the application's mode transition |

Qt keeps semantic move history in MainWindow. Spectrum's bounded GUI_LOG rows
are formatted display state, not a semantic game-history database: they are
never consulted for legal moves, undo, persistence or session ply. Its
`last_ply_seen` is a display numbering fallback only. This refactor does not add
a full move-history store to Spectrum or pretend those rows can reconstruct it.
Command-entry recall belongs to UX on both targets.

## Implemented result

The measurements below record the initial same-unit implementation, superseded
for board interaction by the linked-owner cut described at the end.

All four implementation cuts are present. Qt's `make client-test` passed 13/13
tests. `build/ux-isolation-final-verified.log` records the passing combined
Spectrum host, ABI and size close-out after removing GUI keyboard polling.

| Linked measure | Starting checkpoint | UX implementation |
| --- | ---: | ---: |
| Classic resident | 35179 | 35181 |
| Classic BSS / SP gap | 420 / 1097 | 420 / 1095 |
| Next resident | 32042 | 32044 |
| Next BSS / SP gap | 521 / 4133 | 521 / 4131 |
| Next graphics extension | 4808 | 4808 |

The two resident bytes pay for the explicit interaction/application boundary.
No duplicate state, overlay growth, ABI change or baseline increase was needed.
Assembly fragments preserve instruction placement within their owning assembly
units; whole artifacts are not claimed byte-identical because C symbols and the
source-bound atlas fingerprint change.

The integrated SpectraNext target uses the external canonical `port local-test`
gate, with checkpoint source/artifact binding in `build/ux-spectranext-local-test.json`
and command output in `build/ux-spectranext-local-test.log`. This is technical
maintenance validation, not a new port publication or a hardware observation.

## Subsequent Setup repaint correction

The shared Setup coordinator now includes newly visible rows in its render mask
and preserves the unchanged LINK row when CREATE/JOIN invalidates endpoints.
MENU_CONFIG no longer repaints all visible text for an incremental request.
Host scenarios and Classic/Next ASM vectors cover both preserved LINK pixels
and newly revealed options.

The host/guard portion of `build/setup-repaint-full-check.log` passed; its size
gate rejected one additional MENU_CONFIG byte. An equivalent short jump removed
that byte. The focused ASM vectors passed again, followed by the combined
firmware-only ABI/size gate in `build/setup-repaint-firmware-verified.log`.
Final resident sizes are Classic 35252 and Next 32115 (+71 bytes over the UX
checkpoint); BSS remains 420/521, MENU_CONFIG returns to its prior size, and
committed size/ABI baselines remain unchanged.

Classic and Next artifacts include this correction. The SpectraNext package
and its earlier local-test evidence predate this follow-up; its shared source
contains the fix, but its package has not been regenerated for it.

## Linked board-interaction implementation plan

Baseline: main at d3ce2fb plus the preserved, verified UX simplification worktree.
`build/ux-simplify-full-check.log` passed: Classic resident/BSS/SP gap
35252/420/1024; Next 32115/521/4060. No pre-edit firmware rebuild.

1. Transfer the four cursor/selection bytes and operations to
   `ui/board_interaction.c`; keep storage static and resident.
2. Replace textual application ports with ordinary function declarations.
   Application retains admission, legality, hint calculation and ordered sends;
   interaction retains selection, navigation, formatting and presentation calls.
   Borrowed strings live only for the synchronous call. No queue or callback table.
3. Link interaction separately in firmware and host judges. Test navigation,
   selection, cancellation, rejected/failed intentions and mode restoration via
   operations, never by writing private state. Remove obsolete includes.
4. Close with one full-check: shared scenarios, guards, Classic/Next ABI and
   linked sizes. Existing baselines remain unchanged; no Qt rebuild is needed.

| Option | Cost / risk | Decision / acceptance |
| --- | --- | --- |
| Private resident unit, direct queries/intents | Four existing BSS bytes; calls/code delta to measure | Implement; host contracts and linked gate |
| Same-signature Z80 symbol bindings and HL hint coordinates | Avoid wrapper/stack overhead without exposing state | Implement after first linked cut exceeded Classic by 58 B |
| Duplicate fact snapshot or callback/queue framework | RAM, synchronization and indirect calls | Reject |
| Export cursor/selection variables | No enforceable ownership | Reject |

Recovery point is the preserved pre-cut worktree, not a reset to HEAD: reverse
only this cut, preserving the earlier simplification. Scope excludes Qt changes,
editor/session restructuring, new overlays, protocol changes and packaging.

### Linked-owner result

Completed. `build/ux-linked-compact-verified.log` is the successful full-check:
UX scenarios, DIRECT parity, MQTT 77/77 on Classic/Next at 50/60 Hz, guards,
both overlay ABI manifests and linked memory limits pass.

| Measure | Before this cut | Linked owner |
| --- | ---: | ---: |
| Classic resident | 35252 | 35282 |
| Classic BSS / SP gap | 420 / 1024 | 420 / 994 |
| Next resident | 32115 | 32145 |
| Next BSS / SP gap | 521 / 4060 | 521 / 4030 |
| Next graphics extension | 4808 | 4808 |

The first linked implementation added 90 resident bytes and failed the Classic
budget by 58 bytes (`build/ux-linked-verified.log`). Direct symbol bindings,
register argument passing and simpler fact returns recovered 60 bytes within
this boundary. The final +30 bytes are the measured cost of the independent
owner, not a byte-neutral source move. BSS and overlays are unchanged; no size
or ABI baseline was edited. Classic retains 2 resident bytes below its ceiling.

Both production maps mark `cursor_row`, `cursor_col`, `selected_row` and
`selected_col` as local symbols of `board_interaction_c`. The application has no
declarations or direct accesses to them. The read and move-intention aliases
resolve to the exact existing board-reader and local-emitter addresses; they
export operations, never storage. The internal emitter remains local to app_c.
Host tests link interaction as a separate source and use read-only observations
to assert results; initialization and changes use production operations.

Qt was unchanged by this cut; its preceding 13/13 client-test evidence remains
applicable. No additional platform package, launcher, commit or push is part
of this implementation cut.

## Editor storage boundary

The next slice preserves editor behavior, the five-byte resident storage order,
overlay entry contracts and existing limits. Baseline is the linked-owner result
above; no additional pre-edit firmware build was run.

| Option | Cost / risk | Decision / acceptance |
| --- | --- | --- |
| Local ASM storage and existing map imports restricted to INPUT_EDIT | Same five BSS bytes; placement and ABI require linked verification | Implement; editor scenarios, import-filter test and full-check |
| Read-only volatile activity alias | No copied state or accessor call; overlay writes remain observable | Implement; app consumes only this view |
| Remove editor checks after its early-return dispatch | Flip/theme cannot activate INPUT_EDIT; avoids redundant volatile reads | Implement; active/inactive flip regression in DIRECT host judge |
| Move editor operations resident or introduce a new overlay | Resident growth or extra dispatch without ownership benefit | Reject |

`editor_state.inc` is removed. The independent ASM module defines the five raw
names locally; the resident application cannot link to those names. The overlay
builder exposes them only to INPUT_EDIT, which already owns editing and recall.
Host fixtures supply test-only storage and exercise unchanged production editor
operations. Rollback reverses this slice alone, preserving prior UX changes.

At that checkpoint, Setup was a separate remaining boundary: its UI state shares storage with CONFIG
and RESTORE, and app.c still contains Setup composition and focus writes. This
editor slice does not claim to close those responsibilities.

The first full-check passed host/guard/ABI checks but measured +3 resident bytes
(Classic 35285), one byte above the unchanged ceiling. Reordering the first
keyboard condition did not reduce the Next link, so that retry was stopped.
Two redundant activity reads after editor dispatch were then removed: an active
editor returns before those flip/theme branches, whose presentation operations
cannot activate it. The focused DIRECT judge covers active/inactive flip routing.

Final evidence: `build/ux-editor-key-test.log` passes DIRECT at 50/60 Hz on both
host target variants. `build/ux-editor-firmware-verified.log` passes the combined
ABI/size gate for Classic and Next. Earlier unaffected host/guard results from
`build/ux-editor-full-check.log` are reused, including editor scenarios and MQTT
77/77. No baseline was changed.

| Measure | Before editor cut | Final |
| --- | ---: | ---: |
| Classic resident / BSS / SP gap | 35282 / 420 / 994 | 35273 / 420 / 1003 |
| Next resident / BSS / SP gap | 32145 / 521 / 4030 | 32136 / 521 / 4039 |

Both maps confirm five consecutive local symbols in `editor_state_asm`.
The public view is a read-only alias of the existing activity byte; a host
compile-negative check rejects assignments through its declaration. Overlays
and the 4808-byte Next graphics extension are unchanged. The net resident
reduction is 9 bytes per target, including the redundant-check removal.

## Final Setup/composition slice

Baseline: preserved dirty tree after editor closure, Classic 35273/420/1003 and
Next 32136/521/4039 (resident/BSS/SP). This slice moves Setup composition,
focus updates and visual feedback to a separately compiled UI owner. Application
ports retain session start, config save, clock effects and piece loading. No
protocol, configuration format, repaint sequence or overlay entry changes.

| Option | Cost / risk | Decision / acceptance |
| --- | --- | --- |
| Reuse CONFIG/RESTORE workspace with private Setup aliases and read-only view | No duplicate BSS; preserve 55-byte order and phase lifetime | Implement; map/ABI and overwrite/reconstruction tests |
| Compile Setup composition separately, synchronous application effects | Calls/register costs measured at closure | Implement; Setup scenarios and full-check within existing limits |
| Bounded notice table in a register-argument helper | Replace five repeated notification branches; no mutable storage | Implement after the first link added 29 bytes; exhaust all 256 notice values |
| New context allocation, overlay, callbacks or generic event bus | Added RAM/dispatch without an existing need | Reject |

Rollback reverses only this slice against the preserved editor result, not HEAD.
Qt, launchers, packaging of additional platforms, commits and pushes are excluded.

### Closure

The agreed Spectrum UX isolation is complete: board interaction is independently
compiled with private state, editor storage is linker-local with owner-only
overlay imports, and Setup composition now lives in `ui/setup.c`. Application
ports retain non-visual effects. Setup's existing 55-byte phase-shared workspace
is preserved, not copied; its raw ASM aliases are local and imported only by
MENU_CONFIG, SETUP, INPUT_EDIT and TIME_CONFIG. The public C view is read-only.

| Measure | Before Setup cut | Final |
| --- | ---: | ---: |
| Classic resident / BSS / SP gap | 35273 / 420 / 1003 | 35280 / 420 / 996 |
| Next resident / BSS / SP gap | 32136 / 521 / 4039 | 32143 / 521 / 4032 |

Net cost is 7 resident bytes per target; BSS, overlays and the 4808-byte Next
graphics extension are unchanged. Baselines and overlay entry ABI are unchanged.
The initial +29-byte link was reduced by 22 bytes using the bounded notice helper.

Evidence: the host/guard portion of `build/ux-setup-verified.log` passed before
the first firmware compile exposed an ASM-binding placement error. After that
fix and notice compaction, `build/ux-setup-final-verified.log` passed the combined
`abi-check size-check nex-size-report` gate. `build/ux-setup-final-host.log` passes
Setup scenarios and DIRECT/MQTT parity, including all 77 MQTT transcripts per
variant. Setup tests cover incremental repainting, save/reconstruction, resource
failure, navigation and all 256 notice values; the native ASM vector checks the
workspace writes and preserved registers.

The final portable host fixture adjustment is conditional on
`NETCHESSZX_HOST_SESSION_TEST`. Target preprocessing of app and Setup was
compared before/after that adjustment and is identical, so the passing firmware
evidence is reused without another link.

## Chess-independent board interaction

Approved slice: remove chess cell encoding, algebraic formatting, automatic
promotion and preferred starting-square policy from the existing compiled
interaction owner. Preserve the 8x8 navigation, notices, repaint order, automatic
queen promotion, protocol bytes and domain mutation only after peer acceptance.
No new promotion chooser or configurable-grid framework is introduced.

Baseline is the preserved Setup closure (Classic 35280/420/996; Next
32143/521/4032, resident/BSS/SP), not HEAD. Recovery copies of the four production
files were taken in `build/ux-domain-before/`; rollback is limited to this slice.

| Option | Cost / risk | Decision / acceptance |
| --- | --- | --- |
| Replace raw cell/side queries with selectability and coordinate intentions | Direct resident calls; move formatting and notice buffers leave UX | Apply; domain-free interaction test plus productive app adapter tests |
| Application supplies cursor home square through existing operation | Two-byte HL argument, four private UX bytes unchanged | Apply; white/black preferred, scanned and fallback positions |
| Preserve automatic queen policy in application, legality in board/session | No new state, chooser, protocol or overlay | Apply; both promotions and wait-for-ACK checks |
| Dynamic grid, callbacks, generic engine or new overlay | Extra code/placement without a present need | Reject |

All code remains resident; no ISR, paging, scratch or overlay-entry change.
Coordinates are row in low byte and column in high byte; single-square operations
use existing z88dk fastcall, the two-square move intent uses ordinary C ABI.
Callers supply valid 8x8 coordinates. Acceptance: focused interaction/DIRECT tests
then one full-check, retaining committed memory and ABI baselines. Costs include
linked CODE/BSS/SP/overlays; no cycle or dynamic stack improvement is claimed.

Closure: `build/ux-domain-full-check.log` completed with exit 0. Host scenarios,
ASM vectors, dependency guards, DIRECT parity, MQTT 77/77 in all four variants,
both overlay ABI manifests and linked size limits pass. The focused baseline
and post-change interaction/DIRECT checks also passed. No baseline was edited.

| Measure | Before adapter cut | Final |
| --- | ---: | ---: |
| Classic resident / BSS / SP gap | 35280 / 420 / 996 | 35235 / 420 / 1041 |
| Next resident / BSS / SP gap | 32143 / 521 / 4032 | 32098 / 521 / 4077 |

Net saving: 45 resident bytes per target; BSS, overlay sizes and Next graphics
extension are unchanged. Both maps retain four linker-local interaction bytes;
the selectable-cell, notice and coordinate-intent adapters belong to `app_c`.
The isolated interaction test uses non-chess cell values and observes numeric
intentions. Productive application tests check both colors' preferred/scanned/
fallback squares, exact notices, automatic promotion and no mutation before ACK.
This closes the approved chess-convention removal, not a generic engine or a
claim that Spectrum rendering can be copied to a different graphics backend.

Pre-publication closure: the additional Setup C/ASM layout guard passes
`make ux-boundaries-check`, including offset, added-field, widened-field and
unknown-type negative cases. Alias ordering and editor zero/nonzero activity
semantics are documented. Only tooling/tests and documentation changed after
the firmware gate above. Qt sources still match the passing 13/13 client tests
in `build/ux-simplify-client-test.log`; those results are reused. There are no
remaining implementation tasks in the agreed UX isolation scope.

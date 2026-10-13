# Maintenance Contract

This document is the current entry point for changing Shatranj safely. It
describes process, not product behavior; the contracts below remain
source-of-truth for their own domains.

## Authoritative Documents

- `wire-contract.md`: bytes, routes, verbs, limits, and compatibility rules.
- `session-core-contract.md`: session states, timers, correlation, and actions.
- `source-layout.md`: ownership and allowed dependency direction.
- `architecture-decisions.md`: accepted structural decisions and rejected
  alternatives.
- `post-refactor-backlog.md`: active, optional, and hardware-only follow-up.
- `cross-port-ledger.md`: what has crossed to or from MirrorShift, what was
  rejected, and why.

`docs/archive/` contains selected historical evidence. Removed handoffs,
versioned proposals, review logs, and scratch notes remain available in Git
history; none override the current contracts.

## Change Workflow

Protocol or session behavior changes in this order:

1. Amend the wire/session contract and state any mixed-version consequence.
2. Add one shared transcript that fails for the missing behavior; do not add
   target-specific expected outcomes.
3. Update the canonical portable reducer.
4. Update the compact Spectrum state machine to satisfy the same transcript.
5. Keep adapters mechanical: translate inputs and execute actions, but do not
   own protocol policy.
6. While editing, run the focused host test for the missing transcript. Close
   the block with the single gate in the validation matrix; do not precede it
   with `abi-check`, `size-check`, or a second `make test`.

Do not weaken a judge to make an implementation pass. A newly exposed defect
stops the change until it is understood.

## Validation Matrix

This matrix is the canonical close-out ladder. Root `AGENTS.md` and the Mex
Spectrum pattern must follow it. Deduplicate by real Makefile effects, not by
target names.

| Change | While editing | When closing the block |
|---|---|---|
| Documentation or cosmetic text | Review the diff | Stop |
| Qt UI or desktop core | Focused desktop test if one exists | `make client-test`. On macOS also `make client` to install the inspectable app. |
| Protocol, chess, or session semantics | Focused host test (`session-*-test` or equivalent) | `make test`. Add `make client-test` only if desktop code changed. Use `make full-check` only when the close-out also needs linked Spectrum ABI/size, or at a release/commit checkpoint. |
| Overlay ASM/C with no entry, API, or layering change | Focused ASM vector or overlay host test | `make overlay-size OVERLAY=NAME` when that overlay can grow (reuses `build/SHATRANJ.map`; does not relink TAP; fails if the config stamp is newer than the map). `make size-check` only when resident limits are also required or the map is missing. |
| Overlay entry, public API, or module layering | `make module-guards` (includes static `overlay-entry-abi-check`) | `make abi-check` once. If size limits also apply, run `make full-check` once instead of `abi-check` and then `size-check`. |
| Resident, banking, or size-sensitive Spectrum | Focused host or ASM test | `make size-check` for Classic limits, or `make full-check` when ABI and host tests are also required. |
| Next-UART diagnostic TAP | — | `make next-size-report` only when that TAP is in the request. It is not part of `full-check` or CI. |
| SpectraNext | — | Only when the request names SpectraNext. It is not part of `full-check`. |
| Release candidate | — | One `make full-check`, `make client-test` if desktop is in scope, and green CI. |

### Close-out rules

- Run one large firmware/host gate per closed block. Do not start
  `full-check`, `abi-check`, `size-check`, `tap`, or `nex` as a session
  baseline before editing.
- Prefer the single Make target whose prerequisites already cover the needed
  effects. Separate `make` processes do not share those rebuilds.
- `make full-check` is `module-guards test abi-check size-check nex-size-report`.
  One invocation rebuilds Classic TAP once and Next NEX once, then runs host
  tests. Do not run those targets as extra makes before or after it.
  Its final recipe also executes the linked BOARD entry in Z80 for both targets
  (`tests/tools/test_board_action_z80.py`). This adds no firmware build. To
  inspect retained artifacts, pass their complete build directory directly to
  that script, plus `--next` for a Next build; do not mix old maps and new OVLs.
- `make abi-check` rebuilds Next NEX and Classic TAP. It is not a Python-only
  guard; `module-guards` already covers static overlay entry ABI. Do not also
  run `make tap`, `make nex`, `make size-check`, or `make nex-size-report`.
- `make size-check` rebuilds Classic TAP. Do not run `make tap` first.
- `make test` and `make module-guards` do not invoke zcc.
- `make overlay-size OVERLAY=NAME` rebuilds one overlay against an existing
  map and checks the 2048-byte cap plus the size baseline. It does not pack
  the atlas or relink TAP. It refreshes `spectrum_config.json` and fails if
  the map is missing or older than that stamp.
- `make next-size-report` builds a third firmware (`build/next/`, Next-UART
  TAP) that CI does not run.

`make full-check` remains the canonical Spectrum/host close-out when the block
needs module boundaries, shared transcript tests, classic and Next overlay ABI,
size contracts, and NEX packaging together. CI also runs the Qt client on
Linux, macOS, and Windows.

A software gate cannot stand in for real hardware. Missing hardware evidence is
reported as pending, never inferred as passing.

## Toolchain Reproducibility

The Spectrum CI image is pinned by digest; do not replace it with a mutable
Docker tag. Local linked-size baselines are also verified with z88dk revision
`762578732c`. The generated Classic and Next configuration stamps record the
actual `zcc` and `z80asm` identity strings, so changing either executable
invalidates cached firmware instead of silently reusing incompatible output.

Before accepting a local size-baseline change, confirm that the first line of
`zcc` identifies revision `762578732`. A different revision may be useful for
an experiment, but its byte deltas do not justify changing a baseline.

## Version Source

`VERSION` at the repository root is the only editable product version. It
accepts `major.minor[-devNNN|-devESP]` or
`major.minor.patch[-devNNN|-devESP]`; CMake keeps numeric desktop bundle
metadata while showing the suffix in About. Spectrum
shows development versions directly in its fixed banner slot and keeps the
`version` prefix for releases. Do not duplicate a version literal in source
code or build files.

## Real-Hardware Validation

Real-hardware results belong in [`hardware-validation.md`](hardware-validation.md),
the issue, or the commit they support, not in a permanent directory of run
logs; release notes summarize them. Record concise pass, fail, or pending
results together with:

- commit and artifact hashes;
- 48K, Next-TAP, or NEX target and Next core/firmware when applicable;
- ESP firmware, UART backend, transport, and host/guest role;
- boot, asset load, palette, piece set, and About-screen results;
- LOCAL/IP/port editing, cursor behavior, themes, hints, and START;
- DIRECT and MQTT in both roles;
- MOVE, chat, DRAW, RESET, RESIGN, TAKEBACK, RESTORE, reconnect, and liveness.

Keep raw logs, captures, and photographs outside the repository unless one is
required as durable evidence for a specific defect.

For Next banking changes, repeat palette/set changes and About open/close cycles
to exercise bank restoration and interrupt state.

## Refactoring Threshold

Large files alone are not a reason to add layers. Extract another component only
when it creates a testable ownership boundary needed by an actual change. The
next likely seam is the Qt broker/socket lifecycle, but it remains deferred
until a feature or defect cannot be isolated cleanly with the current desktop
controller and adapters.

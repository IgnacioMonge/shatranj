# Shatranj technical documentation

[Español](README.es.md) · [User guide](../README.md) · [Qt client guide](../client/README.md)

This directory contains the maintained engineering documentation for Shatranj
1.3. The two normative contracts are deliberately transport- and client-neutral:

- [`wire-contract.md`](wire-contract.md) — payloads, Direct newline-delimited
  TCP framing, MQTT topics, restore exchange, and compatibility rules.
- [`session-core-contract.md`](session-core-contract.md) — session state,
  reducers, retries, acknowledgements, and cross-client semantics.

## Architecture and ownership

- [`source-layout.md`](source-layout.md) — module boundaries and ownership.
- [`architecture-decisions.md`](architecture-decisions.md) — accepted
  cross-cutting decisions and their rationale.
- [`maintenance.md`](maintenance.md) — supported build, validation, and
  release-maintenance rules.

## Current state and follow-up

- [`handoff.md`](handoff.md) — where the project stands, what is validated
  where, and the single list of pending work. Read it first when resuming.
- [`hardware-validation.md`](hardware-validation.md) — real-hardware results
  with builds and artifact hashes.
- [`post-refactor-backlog.md`](post-refactor-backlog.md) — numbered requests,
  decisions and optional experiments.
- [`game-stats.md`](game-stats.md) — end-of-game statistics and the
  host-only restart flow, with the verification record.
- [`game-journal-design.md`](game-journal-design.md) — on-disk move journal
  and STJ1 saves.
- [`game-recovery-experiment.md`](game-recovery-experiment.md) and
  [`game-recovery-handoff.md`](game-recovery-handoff.md) — interrupted-game
  recovery: design, costs, hardware evidence and pending tests.
- [`spectrum-connection-config.md`](spectrum-connection-config.md) — Setup
  screen, RTC fallback and SAVE-state invariants.
- [`spectranext-direct.md`](spectranext-direct.md) — Spectranext direct
  HTTP(S) launch: package, resource lifetime and publication rules.
- [`zesarux-zxespemu.md`](zesarux-zxespemu.md) — local ZEsarUX and virtual
  ESP-UART harness.

The implementation is shared across Qt Windows/macOS/Linux, ZX Spectrum
Classic, Spectrum Next, and SpectraNext. Common C owns chess, protocol, session, and save
format rules; desktop code adapts them to Qt and the Spectrum clients use the
compact target-specific runtime. Keep protocol parsing/building in common code
and treat the two contracts above as the source of truth.

## Build and validation entry points

Run from the repository root:

```sh
make test          # host tests
make client-test   # Qt build and tests
make tap           # Classic TAP/OVL/DAT
make nex           # Next self-contained NEX
make full-check    # release-level cross-target guards
```

Spectrum configuration variables are `PORT`, `MQTT_HOST`, `MQTT_PORT`, and
`MQTT_CODE`; use them only for a configured Spectrum build. The Linux Qt client
is supported and tested against the system Qt. The **Build Linux AppImage**
workflow packages x86_64 and keeps manual-run artifacts; attaching them to a
published release is a separate manual step. The public Qt workflow is CMake through the repository
targets; do not document a qmake or raw CMake fallback.

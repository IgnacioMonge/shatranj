# Changelog

All notable changes to Shatranj are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com). Version 1.0 is
the first public release, so this entry covers the full preparation of the
release rather than changes since a previous tagged version.

---

## [1.0] - 2026-06-25 - Opening Move

Initial public release of Shatranj, an online chess application for a real ZX
Spectrum 48K. The release includes the Spectrum client, the Qt PC client, Direct
TCP play, MQTT play, runtime assets, build guards, and the hardware-facing work
needed to make the application usable on the 48K target.

### Added

- Full ZX Spectrum 48K client with board rendering, cursor input, text input,
  move entry, clocks, move history, chat, status notices, setup screens, and
  game-state transitions.
- Spectrum-to-Spectrum and Spectrum-to-PC play using the same application-level
  protocol.
- Qt PC client with interactive board, legal-target display, move history, chat,
  clocks, connection setup, status messages, RX/TX protocol log, and About
  dialog.
- Direct TCP transport for reachable peers.
- MQTT transport for broker-mediated games across NAT or CGNAT.
- Host and Guest roles with side negotiation, game-start acknowledgement, session
  identity, generated room codes, and reconnect handling.
- Human-readable wire messages for setup, join, game start, moves, ACK/NACK,
  chat, ping, reset, draw, resign, BYE, and reconnect paths.
- Application-level ACK/NACK handling so peers agree on game state instead of
  relying only on transport delivery.
- Connection Setup and Game Setup menus for transport, endpoint, port, room code,
  role, side/color policy, notation, board theme, piece set, and hints.
- Spectrum-native chat with side icon, timestamp, wrapping, and a fixed two-line
  message envelope shared by Spectrum and PC input limits.
- `/draw` and `/resign` commands with confirmation and opponent notification.
- Restart, reset, disconnect, opponent-ready, check, checkmate, and rematch
  flows in the Spectrum UI and PC client.
- Independent Spectrum SAN notation, move display, and pending-move feedback.
- Optional Spectrum legal-move hints with explicit Send Move confirmation.
- Check indicators and terminal status presentation for checkmate/rematch states.
- Three 16x16 piece sets: BRRY, SPCY, and PIXL.
- Five board palettes: Classic, Blue, Green, Cyan, and Magenta.
- DAT-backed runtime asset pack for Spectrum UI data, piece graphics, logo/about
  artwork, and runtime resources.
- esxDOS/divMMC overlay system for cold code paths within a fixed 2 KB overlay
  slot.
- Handwritten Z80 support for screen rendering, input polling, UART, text paths,
  overlays, board helpers, and rules-facing glue.
- Fixed low-RAM layout for move log, chat log, clocks, board state, overlay
  context, hints, and transport-visible state.
- Build guards for overlay size, overlay entry ABI, SDCC/IY contract, module
  layering, low-RAM overlap, resident size, BSS tail, and stack margin.
- Windows PC packaging path plus macOS/Linux Qt build support through CMake/qmake
  paths.

### Changed

- Renamed the project from its development name, NetChessZX, to Shatranj.
- Standardized application versioning at `1.0` for both Spectrum and PC builds.
- Made the SDCC/IY Spectrum backend the supported build path.
- Dropped the legacy sccz80 Spectrum build path.
- Unified Direct and MQTT session handling behind a shared game/session layer.
- Split common game payload grammar, MQTT session grammar, and keepalive grammar
  out of transport-specific code.
- Reworked MQTT peer identity around role and session id instead of machine
  origin.
- Moved transport liveness ownership into the net/session layer.
- Moved resident setup logic, board apply paths, move/chat log paths, MQTT
  connect/tx helpers, and other cold paths into overlays.
- Externalized runtime art and UI data into the DAT asset pack instead of keeping
  it in resident code.
- Centralized Spectrum layout constants and named screen zones for board, chat,
  clocks, status, setup, and move log areas.
- Centralized fixed low-RAM addresses and tightened overlay include/import
  boundaries.
- Unified user-facing notice strings across Direct and MQTT paths.
- Preserved pre-game chat when a game starts; game start now resets game state
  and move history, not the conversation that led to the game.
- Made board cursor mode and text-input mode explicit so setup editing and board
  play do not fight for input.
- Improved PC move-history formatting, pending-state feedback, disconnect status,
  and chat UI text.

### Fixed

- Fixed false Direct disconnects caused by valid peer message bursts and chat
  payloads near the Spectrum message envelope limit.
- Fixed Direct move delivery and Direct setup display/status handling.
- Fixed Direct HELLO/session handshake recovery without ping-pong loops.
- Fixed Direct guest early reconnect, reset, reconnect, and connection-wait
  cancellation paths.
- Fixed MQTT CONNECT packet header copy and MQTT host/guest negotiation.
- Fixed MQTT room-conflict recovery and stale retained-payload recovery.
- Fixed MQTT session startup, session IDs, publish acknowledgement handling,
  unusable publish ACKs, and background UART draining.
- Fixed game-start acknowledgement and reset-before-game-start edge cases.
- Fixed mate rematch flow, terminal status, check/checkmate presentation, and
  remote move validation.
- Fixed promotion handling and queen-only promotion error feedback.
- Fixed Spectrum game-loop state recovery after communication failures and UI
  transitions.
- Fixed Spectrum UI state recovery, board snapshots at game start, status overlay
  rendering, status clock clearing, setup hints visibility, setup sprites, notice
  bounds, and chat text rendering.
- Fixed hint repaint and validation workflow around Send Move confirmation.
- Fixed PC client pending-state feedback, move notation display, move-history
  table formatting, peer timeout, and session handling.
- Hardened chat input limits so the PC client cannot send more text than the
  Spectrum two-line chat envelope can display.
- Hardened AT command construction, IP/SNTP parsing, setup rendering, line buffer
  sizing, and malformed modem response handling.
- Hardened UART receive caching and bounded background drain paths around overlay
  latency and short bursts.
- Hardened overlay loading, overlay seek paths, overlay ABI guards, resident
  state imports, and build portability checks.
- Hardened architecture boundary checks, size report metrics, integration guard
  baselines, and SDCC/IY migration gates.

### Optimized

- Shrunk the resident Spectrum binary repeatedly to keep the 48K target viable.
- Shrunk rules, board apply, session dispatch, status, SAN, SNTP, MQTT buffers,
  MQTT warm-up, setup logic, and resident protocol/text paths.
- Optimized keyboard scanning, horizontal-line loops, fast 64-column line
  rendering, screen address calculation, render hints, setup painting, and text
  helpers.
- Optimized piece sprite lookup and piece sprite blitting with measured Z80
  hot-path improvements.
- Moved additional transport and setup work into overlays to reduce resident RAM
  pressure.
- Removed obsolete feature fallbacks, dead inline MQTT transport forks, unused
  MQTT RX overlay code, and legacy info-panel exports.

### Release Artifacts

Spectrum release files:

- `SHATRANJ.tap`
- `SHATRANJ.OVL`
- `SHATRANJ.DAT`

PC client package:

- `shatranj-client.exe`

### Known Limitations

- Spectrum promotion UI supports queen promotion only in 1.0.
- MQTT retained game-state restoration is not part of 1.0.
- The Spectrum build targets 48K machines, so contended RAM, overlay latency, and
  tight resident memory remain deliberate constraints.
- Direct TCP still requires reachable peers; MQTT is the intended path when NAT
  or CGNAT prevents direct connectivity.

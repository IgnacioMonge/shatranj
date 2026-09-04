# Changelog

User-visible changes to Shatranj are documented here. Developer and maintenance
details live under [`docs/`](docs/).

---

## [1.2] - 2026-09-04 - Connected Rooks

Shatranj 1.2 adds a native Spectranext cartridge edition and improves setup,
presentation, networking, and saved-game reliability across every supported
platform.

### Highlights

- Added Spectranext support with Direct TCP, MQTT, cartridge clock, persistent
  settings and saved games, and a guided resource installer.
- Added persistent Spectrum setup for connection, clock, color, notation,
  board theme, piece set, and legal-move hints.
- Added live board and piece previews during Spectrum setup.
- Made Spectrum setup, typing, theme changes, and board updates faster and
  smoother.
- Refreshed the Qt desktop interface and improved responsiveness during games.
- Improved Direct, MQTT, reconnection, saved-game restoration, takeback, and
  rematch behavior across mixed-platform games.

### Added

#### Spectranext cartridge edition

- Native Spectranext client for a ZX Spectrum fitted with the cartridge.
- Direct TCP and MQTT play through the cartridge's network connection.
- Cartridge clock support.
- Persistent connection settings and local saved games.
- Guided installation from the public Spectranext resource URL; reinstalling
  the resource preserves settings and saved games.
- Spectranext-specific title screen and opponent-platform identification.

#### Spectrum setup and presentation

- Persistent CONNECTION SETUP and GAME SETUP choices.
- Guided setup that reveals each required choice in order and shows SAVE, EDIT,
  and START only when applicable.
- Editable Direct address and port, plus validated MQTT room entry.
- Live previews for board themes and piece sets.
- Startup clock status on Classic and Next.
- Opponent-platform display for Classic, Next, Spectranext, Windows, macOS, and
  Linux; older peers appear as `?`.
- Full-color About screen on Classic.
- Desktop action buttons for draw, takeback, and restart.

### Changed

- Spectrum setup navigation and typing respond more reliably to fast input.
- Board-theme, piece, move-list, chat, clock, cursor, and prompt updates are
  faster and show less flicker.
- Returning to setup refreshes connection information without unnecessarily
  redrawing the startup panel.
- When no hardware clock is available, Spectrum falls back to the last saved
  UTC offset.
- The Qt client has refreshed connection panels, board presentation, title bar,
  icons, dialogs, and status messages on Windows, macOS, and Linux.
- Desktop board animation and network activity remain more responsive during
  play.
- Log timestamps follow the user's locale.
- Either player may request restoration of a saved game when it matches the
  current seating.
- Direct and MQTT connections recover more reliably when a player disconnects,
  reconnects, or rejoins.
- Next can recover an ESP module left at the wrong baud rate or temporarily
  unresponsive.

### Fixed

#### Spectrum setup and display

- Fixed GAME SETUP choices disappearing, failing to enable SAVE, or not being
  restored after changing connection mode.
- Fixed SAVE/START focus and visibility after configuration saves.
- Fixed board-theme selection and address/port editing leaving incorrect text,
  colors, or cursor marks on screen.
- Fixed invalid Direct and MQTT values being accepted or reopening incorrectly
  for editing.
- Fixed stale move hints painting arbitrary squares after a cold boot.
- Fixed restored move markers, flipped boards, legal-move hints, turn labels,
  and move-list cursors displaying incorrectly.
- Fixed visual artifacts when opening or closing the Classic About screen.
- Fixed moved pieces retaining the previous piece set after changing it.

#### Networking and games

- Fixed background clock synchronization interfering with configuration or
  saved-game operations on Classic and Next.
- Fixed malformed MQTT or Direct traffic disrupting active games or hiding the
  next valid message.
- Fixed a corrected move being rejected after an earlier invalid attempt.
- Fixed simultaneous actions being lost, duplicated, or applied out of order.
- Fixed game start, restoration, resignation, and rematch retries leaving the
  two players in different states.
- Fixed MQTT room ownership, rejoining, and peer-loss cleanup.
- Fixed unrelated incoming Direct connections disturbing an active game.
- Fixed incomplete coordinate moves being handled incorrectly.
- Fixed restore and reconnect timing differences between Classic, Next,
  Spectranext, and desktop peers.

#### Saved games, settings, and clocks

- Made Classic, Next, and Spectranext settings and saved games safer against
  interrupted writes.
- Prevented a failed save from destroying the previous contents of a slot.
- Preserved game and move clocks when saving, loading, or restoring on Spectrum.
- Corrected clock cadence on 60 Hz Next systems.
- Added confirmation before erasing a Spectrum save.
- Fixed failed or repeated restoration transfers leaving players on different
  positions.
- Fixed desktop save filenames when the system clock is invalid or outside the
  supported year range.

#### Qt desktop client

- Fixed cancelling a remote move animation leaving the interface busy or
  applying a move after the game had changed.
- Fixed takeback requests using an outdated move number.
- Fixed missing packaged piece assets, timestamp locale, and macOS window
  initialization.
- Fixed Direct and MQTT disconnect reasons being reported incorrectly.

### Compatibility notes

- Settings saved by 1.1 are reset once because 1.2 stores additional choices;
  review them and select SAVE to keep the new configuration.
- The `.stj` saved-game format remains compatible across Classic, Next,
  Spectranext, and Qt clients.
- Opponent-platform identification is optional; 1.1 peers remain compatible and
  appear as `?`.
- Spectranext installation requires the latest stable cartridge firmware.

### Release artifacts

- ZX Spectrum Classic: `SHATRANJ.tap`, `SHATRANJ.OVL`, and `SHATRANJ.DAT`.
- ZX Spectrum Next: self-contained `SHATRANJ.nex`.
- Spectranext: guided cartridge resource installer.
- Desktop: Windows portable package, macOS application archives, and Linux
  AppImage.

### Known limitations

- Spectrum clients promote pawns to a queen; the Qt client also offers rook,
  bishop, and knight.
- Restoring a saved game requires its host color to match the current seating.
- Direct TCP requires the guest to reach the host's address and port.
- The Linux AppImage is available for x86_64 systems.
- Classic requires matching TAP, OVL, and DAT files. The Next NEX is
  self-contained.
- Classic and Next network play require ESP-AT 1.7.6. Spectranext uses the
  cartridge's own firmware.

---

## [1.1] - 2026-08-04 - Across the Board

Shatranj 1.1 expands the original ZX Spectrum 48K and Windows pairing into one
interoperable application for Classic, Spectrum Next, Windows, macOS, and
Linux.

### Highlights

- Added a native Spectrum Next edition distributed as one self-contained NEX.
- Unified the Qt client across Windows, macOS, and Linux.
- Added negotiated takebacks, stalemate detection, portable saved games,
  synchronized restoration, and post-game rematches.
- Added more board themes and piece sets for desktop and Next.
- Improved Direct and MQTT reliability, reconnection, and peer-loss handling.

### Added

- Spectrum Next graphics using hardware sprites, three piece sets, five RGB333
  board themes, and a full-screen About presentation.
- Next hardware-clock support with network-time fallback.
- Five desktop piece sets and five selectable board textures.
- Persistent desktop connection settings and recent Direct addresses.
- Ten local saved-game slots on Spectrum and a matching desktop Saved Games
  dialog.
- Portable `.stj` saved games containing the complete playable position and
  clocks.
- `/takeback` requests with opponent approval.
- Desktop promotion choice for queen, rook, bishop, or knight.
- Native macOS packages for Apple Silicon and Intel, plus Linux support and an
  x86_64 AppImage.

### Changed

- Resignation ends the game immediately without requiring approval.
- Duplicate or simultaneous requests no longer open repeated prompts.
- Accepted restarts and post-resignation rematches synchronize both players.
- MQTT keeps the remaining player connected when the opponent leaves, allowing
  another player to join.
- Spectrum menus, file browsing, board updates, chat, and status feedback are
  faster and clearer.
- The desktop layout, connection controls, game feedback, history, chat, About
  screen, and platform integration were refreshed.

### Fixed

- Fixed Direct connection, handshake, keepalive, timeout, and reconnect errors.
- Fixed MQTT startup, room conflicts, rejoining, peer departure, and crossed
  game-action errors.
- Fixed takeback and saved-game restoration failures or disagreements between
  peers.
- Fixed Spectrum file-browser, save validation, board-flip, hint, move-list,
  chat, and game-end display problems.
- Fixed Next board graphics, colors, saved-position redraws, and modem recovery.
- Fixed desktop shutdown, connection readiness, game-over actions, geometry,
  and package contents.

### Compatibility notes

- Core 1.0 games remain compatible. Takeback, saved-game restoration, and
  synchronized rematches require peers that support the 1.1 exchanges.
- Do not mix Classic TAP, OVL, and DAT files from different builds.
- The Next NEX is self-contained and must be replaced as one file.

### Release artifacts

- ZX Spectrum Classic: `SHATRANJ.tap`, `SHATRANJ.OVL`, and `SHATRANJ.DAT`.
- ZX Spectrum Next: `SHATRANJ.nex`.
- Desktop: Windows portable package, macOS application archives, and Linux
  AppImage.

### Known limitations

- Spectrum promotion is queen-only.
- Saved-game restoration requires the save's host color to match the current
  seating.
- Direct TCP requires a reachable host.
- The Linux AppImage is available for x86_64 systems.
- Classic and Next network play require ESP-AT 1.7.6.

---

## [1.0] - 2026-06-25 - Opening Move

Initial public release of Shatranj for ZX Spectrum 48K and Windows.

### Added

- Network chess between Spectrum and desktop clients.
- Direct TCP and MQTT connection modes with Host and Guest roles.
- Complete chess rules, clocks, move history, chat, check, checkmate, draw,
  resignation, restart, and rematch handling.
- Spectrum setup for connection, color, notation, board theme, piece set, and
  legal-move hints.
- Three Spectrum piece sets and five board palettes.
- Qt desktop client with interactive board, legal-target display, connection
  setup, history, chat, clocks, status, and protocol log.
- `/draw` and `/resign` commands.

### Changed

- Renamed the project from NetChessZX to Shatranj.
- Preserved pre-game chat when a game starts.
- Improved desktop move history, connection feedback, and chat presentation.

### Fixed

- Fixed Direct and MQTT startup, move delivery, reconnection, and timeout
  problems found before the first public release.
- Fixed game start, reset, rematch, checkmate, promotion, and remote-move
  handling.
- Fixed Spectrum setup, board, hints, status, clock, and chat display problems.
- Fixed desktop move notation, history, pending-action feedback, and peer
  timeout handling.

### Release artifacts

- ZX Spectrum 48K: `SHATRANJ.tap`, `SHATRANJ.OVL`, and `SHATRANJ.DAT`.
- Windows: portable `shatranj-client.exe` package.

### Known limitations

- Spectrum promotion is queen-only.
- Saved-game restoration and takeback are not available in 1.0.
- Direct TCP requires a reachable host; MQTT is the alternative when direct
  routing is unavailable.

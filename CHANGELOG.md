# Changelog

User-visible changes to Shatranj are documented here. Developer and maintenance
details live under [`docs/`](docs/).

---

## [1.3] - 2026-10-13 - Sealed Move

Shatranj 1.3 lets Stockfish play from the desktop client, resumes interrupted
games on every Spectrum, adds player names and end-of-game statistics, and lets
the Spectranext edition start straight from the web. Play is also faster and
steadier on every platform.

### Highlights

- Stockfish joins the game: the Qt client installs the official engine and lets
  it play your side at levels 0-20, against any Spectrum or desktop opponent.
- Interrupted games come back: Classic, Next, and Spectranext resume a game
  after a crash, reset, or power cut, with the board, clocks, and move list.
- Player names of up to ten characters appear in both players' move lists.
- End-of-game statistics on Spectrum and Qt: time per move, longest and
  fastest moves, and every captured piece.
- Spectranext can start Shatranj from its web address without installing it;
  the guided installer remains available.
- A Qt Preferences dialog and a bilingual English/Spanish manual.
- Faster, steadier play: quicker move replies, fewer disk loads on Classic,
  clocks that keep time during disk access, and requests that expire cleanly.

### Added

#### Stockfish in the desktop client

- The Qt client installs the latest official Stockfish release for the
  computer from GitHub, verified by SHA-256 and installed for the current user
  only. It can also update or remove the engine, or use an existing Stockfish.
- Stockfish can play your side at levels 0-20. The knight button beside the
  settings button switches it on or off; while it plays, the button, the board
  frame, and the turn label turn violet and the status bar shows
  `STOCKFISH L<n>`.
- An optional pause of up to 30 seconds before each engine move
  (**Preferences > Engine**) leaves the opponent time to ask for a takeback.
- The opponent sees `STOCKFISH` as the player name and a chat line such as
  `STOCKFISH 19 PLAYS WHITE - LEVEL 5`; Spectrum clients add it to the
  opponent's platform, as in `VS MAC: STOCKFISH`. The choice is fixed for the
  session.
- While the engine plays, the takeback, draw, and resign decisions belong to
  it. The operator can still chat and answer the opponent's requests.
- If the engine stops unexpectedly, the client restarts it once; if it stops
  again, the turn label reads `STOCKFISH STOPPED` and the operator can resign
  or disconnect.

#### Players and statistics

- Player names of up to ten characters (`A-Z 0-9 - _`): Spectrum Setup has a
  NAME row, and Qt sets the name in **Preferences > Player**. Both clients show
  the two names in the MOVES column headers instead of WHITE and BLACK.
- End-of-game statistics on Spectrum and Qt: each player's completed-turn time
  and share of the clock; average, longest, and fastest moves with their move
  numbers; and the captured pieces with the material each side took, including
  en passant and takebacks. On Spectrum, press S after the game; Qt's
  **Game stats** button works during and after the game. Loaded or restored
  games report a partial sample.

#### Interrupted and saved games

- Classic, Next, and Spectranext resume a game interrupted by a crash, reset,
  or power cut. When the opponent is ready, the host is asked
  `Resume last game? Y/N`; the guest approves the restore as usual, and the
  board, flags, clocks, and move list come back. Choosing to disconnect ends
  the game for recovery; a lost connection, reset, or power cut keeps the offer.
- Spectrum saves keep the complete move list with checksums and are verified
  after writing; a failed save keeps the game and its history. Loading a save
  or recovering a game repaints the move list.

#### Spectranext

- Direct launch: open `https://ignaciomonge.github.io/shatranj/` with
  **Load Resource URL** to play without installing anything. The guided
  installer moves to `https://ignaciomonge.github.io/shatranj/install/`.
  Settings and saved games stay in the cartridge's local storage, shared by
  both.
- Spectranext shows the Shatranj title screen for two seconds while it starts
  (any key skips it).

#### Spectrum interface

- The in-game menu keeps FILE, FLIP, and ABOUT as direct tabs and adds rolling
  NTWRK (disconnect, reset), OFFER (takeback, draw, resign), and THEME (board,
  piece set, hints) groups. Up/Down or `Q`/`A` cycles through a group's
  actions.
- Classic and Spectranext: press C on the About screen to see what is new in
  this release.
- Toggling hints confirms `HINTS ON` or `HINTS OFF`.
- `CONFIG I/O ERROR` reports a settings file that exists but cannot be read.

#### Desktop client

- Preferences dialog (gear button, Ctrl+, or the macOS Preferences menu) with
  Board, Player, and Engine tabs.
- English and Spanish manual with an index, opened with the **?** button or
  F1, and from the matching Preferences section.
- **Game stats** toggle that shows the statistics over the board and updates
  after every move.

### Changed

- Draw, takeback, restart, and load requests share a 60-second answer
  deadline on Spectrum and Qt. An unanswered request is withdrawn without
  ending the session, the opponent's question closes by itself, and a
  takeback accepted at the last moment still completes on both boards.
- Games no longer restart automatically after a draw or resignation. Only the
  host asks for a new game: Spectrum shows the host `S:STATS  RESTART? Y/N`
  and the guest `S:STATS  WAITING OPPONENT`, and Qt enables **Restart Game**
  for the host. Declining a new game, or BREAK, ends the session for both
  players.
- Spectrum acknowledges the opponent's move as soon as it is on the board,
  before updating the move list and its recovery data; on Classic the
  opponent waits about 1.6 seconds less per move.
- Classic loads fewer overlays per move and reads each one with a single disk
  call, so play, setup, and game start respond faster.
- Classic clocks keep running during disk access. On every Spectrum the turn
  timer starts as soon as a move is entered, so both players' statistics agree.
- Spectrum Setup no longer walks Direct players through PORT when the stored
  port is valid, enables SAVE after a piece-set change, and redraws only the
  rows that change.
- The Spectrum saved-game list shows the newest save for each slot, erasing
  refreshes the list in place, and the erase prompt also accepts Enter, Space,
  or Cancel.
- Save timestamps follow time-zone changes and midnight on the live clock.
  Selecting RTC in Setup reads the hardware clock again.
- Loading a game on Spectrum restores the board orientation it was saved with.
- Chat and commands wait for the acknowledgement of a pending move instead of
  sending a second message.
- The Qt status bar is shorter and clearer. It names the phase, the pending
  notice, the post-game action, and the opponent's platform, in plain wording
  such as `Opponent offers a draw` and `Connection lost`.
- The Windows and macOS packages include the secure-connection support needed
  to download Stockfish; desktop packages include the manual.

### Fixed

- A takeback requested by the opponent and answered after a few seconds on
  Spectrum no longer turns into a rejection that blocks the board.
- A declined draw no longer reopens when a repeated offer crosses the refusal;
  it stays declined until the position changes.
- A request whose retransmission crosses the answer no longer asks the player
  twice.
- Classic and Next Direct no longer lose a message when a network header
  arrives in pieces while the Spectrum is sending.
- Spectranext Direct accepts each peer on a single socket, so repeated
  reconnections no longer run out of sockets, and a host whose cartridge
  refuses to listen keeps its clock running and accepts cancel.
- Classic retries an overlay that fails to load. If the board code still
  cannot be loaded during a takeback or restore, the game ends with
  `OVL load failed` instead of applying a partial change.
- Qt sends draw, restart, and resignation acknowledgements on the correct MQTT
  topic and reads Direct messages containing stray carriage returns as
  Spectrum does.
- Loading a saved game keeps both clocks exact, and resetting the turn timer
  can no longer shorten the game clock.

### Compatibility notes

- 1.3 plays with 1.2 peers. Player names, request withdrawal, and the
  host-only restart need 1.3 on both sides: a 1.2 peer shows WHITE and BLACK,
  its operator closes an expired takeback question, and it still restarts
  automatically after a draw or resignation, which a 1.3 Spectrum answers with
  `S:STATS ACCEPT RESTART? Y/N`.
- Spectrum settings saved by 1.2 load without a player name. Returning to 1.2
  after saving settings in 1.3 resets them once.
- Spectrum saves use a new format with the full move list, so Spectrum 1.3
  cannot load saves written by 1.2. The Qt client keeps its own save format,
  and restoring a game over the network works as before.
- The Spectranext addresses are now lowercase:
  `https://ignaciomonge.github.io/shatranj/` starts the game without
  installing it, and `.../shatranj/install/` runs the installer. The 1.2
  address, `https://ignaciomonge.github.io/Shatranj/`, no longer works.
- Spectranext 1.3 requires cartridge firmware 1.0 or later, which fixes
  seeking in files read over HTTPS. Older firmware stops at
  `FIRMWARE OUTDATED: SEE README`.
- Spectranext keeps the recovery journal in cartridge flash and writes to it
  after every move.

### Release artifacts

- ZX Spectrum Classic: `SHATRANJ.tap`, `SHATRANJ.OVL`, and `SHATRANJ.DAT`.
- ZX Spectrum Next: self-contained `SHATRANJ.nex`.
- Spectranext: direct launch at `https://ignaciomonge.github.io/shatranj/` and
  guided installer at `https://ignaciomonge.github.io/shatranj/install/`.
- Desktop: Windows portable package, macOS application archives for Apple
  silicon and Intel, and Linux AppImage.

### Known limitations

- Spectrum clients promote pawns to a queen; the Qt client also offers rook,
  bishop, and knight.
- Restoring a saved game requires its host color to match the current seating.
- Direct TCP requires the guest to reach the host's address and port.
- Stockfish plays only from the Qt client, and installing it needs internet
  access to GitHub.
- Only Spectrum clients resume an interrupted game; the Qt client does not.
- The Spectranext direct launch downloads the game at every start, so its web
  address must be reachable; the installed copy starts from local storage.
- Spectranext needs cartridge firmware 1.0 or later.
- Over Direct, Classic and Next can drop chat lines when more than three
  arrive while they are busy; moves are retransmitted.
- The Linux AppImage is available for x86_64 systems.
- Classic requires matching TAP, OVL, and DAT files. The Next NEX is
  self-contained.
- Classic and Next network play require a UART-to-ESP link running ESP-AT
  firmware. Spectranext uses the cartridge's own networking.

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

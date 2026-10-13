# Qt client: Preferences dialog, Stockfish engine option, Help and manual

Date: 2026-10-06. Status: approved in conversation; decisions delegated to the
implementer from this point.

## Goal

Make engine play an explicit, honest, installable option of the Qt client
without adding rows to the main window, replace the settings popup with a real
Preferences dialog, and ship a bilingual in-app manual.

## Main window

- Row 1 unchanged: Direct/MQTT, Host IP or Room, Port, Connect.
- Row 2: Role (Host/Guest), Color (host only), Start Game, Stockfish button
  (♞, checkable, same square style as the gear; shown only when the engine is
  installed and enabled), Preferences button, Help button. While Stockfish
  plays, the bottom bar shows `STOCKFISH L<n>` (revised 2026-10-06 after
  trying a badge, an icon toggle and Player radios).
- The player-name field leaves the main window (moves to Preferences). The
  "Player" row added during prototyping is removed.
- Stockfish badge: own icon (stylised piece with a chip, violet) plus
  `STOCKFISH · L<n>`. Visible only while the engine plays this side. Not
  clickable. The official Stockfish logo is not used (no explicit licence).
- Kept from the prototype: violet board frame, violet destination flash for the
  engine's moves, "STOCKFISH THINKING - <side>" turn label.

## Preferences dialog

Modal `QDialog`, tabs, OK/Cancel, applied on OK. Opened from the gear button,
`Ctrl+,`, and the macOS application menu (`QAction::PreferencesRole`).

- Board: chessboard texture, piece set, legal-move hints.
- Player: name (1-10 of `A-Z 0-9 - _`). Note: visible to the opponent;
  STOCKFISH is used while the engine plays. `STOCKFISH` is rejected as a
  user name.
- Engine:
  - Status: "Not installed" or "<id name> installed".
  - Install / Update to latest: GitHub `releases/latest`, platform asset,
    SHA-256 verified, installed under `AppLocalData/engines/`. Reports
    "already up to date" when the installed tag matches.
  - Uninstall: removes `engines/`, clears the saved path, unticks play.
  - Locate existing: accepts only an engine whose UCI `id name` starts with
    `Stockfish`.
  - Inline progress bar, no extra window.
  - "Stockfish plays my side" (enabled only when installed) and Level 0-20.
  - "How does it work?" opens the manual at the engine section.
- While connected or connecting, the Player and Engine tabs are locked with
  "Locked during a session". Board settings stay editable.

## Engine behaviour and identity

- Preference persisted. At start-up, if enabled, the engine starts and the
  badge shows; if the binary is missing the option is cleared with a notice.
- Connect stays disabled until the engine is running.
- Wire identity: always `MNAME STOCKFISH`; chat
  `<ENGINE> PLAYS <WHITE|BLACK> - LEVEL <n>` once per session, at the first game start or
  after RESTORE completes. Player and level are fixed for the session.
- In engine mode the board refuses clicks and typed moves. The operator can
  still chat and answer the peer's DRAW/TAKEBACK requests; those appear as
  STOCKFISH. Asking for a takeback, offering a draw and resigning belong to
  the engine's side, not to the operator: the Takeback and Draw buttons are
  hidden and `/takeback`, `/draw` and `/resign` are refused (revised
  2026-10-11).
- An engine answer that cannot be played when it arrives (the operator's own
  Draw/Resign/Reset prompt is open, or a takeback, restore or reset overtook
  the search) is dropped and asked for again at the next refresh; closing a
  local prompt triggers that refresh (added 2026-10-11).
- If the engine process dies, the client restarts it once with the same path
  and level, so the announced identity stays true. If it dies again before
  it has moved, the turn label reads `STOCKFISH STOPPED - <side>` and the
  operator resigns or disconnects; the side is never handed back to a human
  mid-session (added 2026-10-11; `pc-engine-flow` covers both with a
  stand-in UCI engine).
- If the peer rejects (NACK) the engine's move three times in the same
  position, the client stops asking and shows the same `STOCKFISH STOPPED`
  label; a new position (takeback, reset, restore) starts a new count. The
  chat announcement retries every second while the game runs until the core
  accepts it. An opponent's TAKEBACK accepted while the engine thinks drops
  the stale answer and the engine answers the opponent's new move (all
  covered by `pc-engine-flow`, added 2026-10-11).
- No Spectrum firmware change. `docs/wire-contract.md` documents the reserved
  name and the announcement.

## Help and manual

- Non-modal help window (Help button, F1): heading index left, text right,
  ES/EN selector defaulting to the system language (Spanish → ES, else EN).
- Opens at a section on request (engine tab link, F1 from Preferences).
- About stays on the main window's banner; the help window's banner is
  decorative (revised 2026-10-06).
- `assets/pc-client/manual/manual.es.md` and `manual.en.md`, rendered with
  `QTextDocument::setMarkdown`. Sections: getting started, connecting, playing
  (moves, chat, commands, clocks, statistics), saving and loading,
  preferences, playing with Stockfish (install, levels, what the opponent sees,
  fair play, licence), playing against ZX Spectrum/Next/SpectraNext,
  troubleshooting (Windows Firewall prompt when hosting, connection errors).
- Client READMEs point to the manual for usage.

## Code structure

- `preferences.{h,cpp}` (desktop core): `ClientPreferences` with
  `load(QSettings&)` / `save(QSettings&)` and name validation. Pure, tested.
- `preferences_dialog.{h,cpp}` (UI): tabs; owns `EngineInstaller`; returns
  `ClientPreferences` on accept.
- `help_window.{h,cpp}` (UI) and `manual_index.{h,cpp}` (desktop core): pure
  heading index and anchors, tested.
- `assets/pc-client/engine/engine-badge.svg`, manuals under
  `assets/pc-client/manual/`, staged by the existing asset step.
- `MainWindow`: remove settings popup, Player row and name field; add badge,
  Help button, F1, `Ctrl+,`; apply `ClientPreferences` on accept.

## Tests

- Existing: `pc-chess-helpers`, `pc-stockfish-release`.
- New: `pc-preferences` (round trip, reserved name), `pc-manual-index`
  (index, anchors, same sections in both languages).
- Adapted: bot identity hook (name, chat, input lock).
- Close-out: `make client-test`, then a loopback game between two dev clients.

## Out of scope

Spectrum firmware, an on-Spectrum manual, macOS shortcuts beyond Preferences,
`macdeployqt` TLS verification (still pending on a Mac).

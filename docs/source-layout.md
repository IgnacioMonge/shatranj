# Shatranj Source Layout

This is the canonical source map. Keep new files inside the matching domain;
do not place loose `.c`, `.h`, or `.asm` files at the old root paths.

## Common Code

- `src/common/space/`: logical cell identities and immutable rectangular
  adjacency, independent of rules, views, sessions and hardware. Currently
  used by the standalone PC/Spectrum demonstrator and the navigation reference
  tests. Spectrum Shatranj uses the ID contract with its specialized 8x8 binding;
  variable rectangle arithmetic stays outside its hot path. Contract and evidence:
  [`game-space-contract.md`](game-space-contract.md).
- `src/common/chess/`: chess position, FEN, legal-move wrapper, and the compact
  C rules implementation used by desktop and host Spectrum tests as the
  reference; shipped Spectrum targets do not link it.
  `game_stats.{h,c}` defines current-game aggregates and the portable oracle
  for SAVELOAD's native Z80 accumulator; metric semantics live in `game-stats.md`.
- `src/common/mqtt/`: MQTT packet encoder/parser used by host tests and PC
  code. Spectrum has its own size-constrained MQTT path.
- `src/common/protocol/`: Shatranj game/session message grammar parse/build
  helpers shared by clients where practical and kept host-testable.
- `src/common/savegame/`: portable save-game wire format shared by desktop and
  Spectrum; filesystem storage remains a desktop-core concern.
- `src/common/session/`: portable DIRECT/MQTT reducers and the canonical
  PC/reference session-policy implementation; host-only and never linked into
  Spectrum targets.

## Spectrum Code

- `src/spectrum/app/`: top-level Spectrum state machine, input flow, turn
  control, application orchestration, and the GUI-independent wall/game clock
  state used by persistence.
  `app/time.*` defines storage and mutating operations; `app/net_runtime.c`
  advances time from the low 16 bits of the ROM frame counter sampled by waits
  and the active session loop, then applies clock/timer commands. UI includes
  only `app/time_view.h` and consumes a const snapshot; RECOVERY uses the same
  read-only view for saved clocks. The legacy overlay tick
  symbol names the application implementation without an extra ABI wrapper.
  Civil-clock, GAME and TURN fractional progress are independent.
- `src/spectrum/config/`: Spectrum options and configuration format. Setup
  choices are temporary inputs, not a second copy of negotiated identity.
  The compatibility header imports the session identity API; storage lives in
  `session/`, not configuration.
  `tools/check_layering.py` separately registers saved options and runtime state
  (RTC availability, palette attributes and hint masks). C and ASM writes are
  allowlisted by file and symbol; address borrowing has a separate read-only
  allowlist. These lexical checks reject new writers/aliases; indirect writes
  through an already reviewed borrow still require code review.
- `src/spectrum/fileui/`: resident file-list navigation and action dispatch
  around the FILEUI overlay; owns selection state and the shared list metadata.
- `src/spectrum/restore/`: resident bridge that passes board snapshots and
  save metadata to the RESTORE overlay for base64 frame build/decode.
- `src/spectrum/saveload/`: resident bridge for SAVELOAD NCZS erase, the
  journal append and (Next) the STJ1 codec entries; Classic routes LOAD/SAVE
  to the JOURNAL overlay here. It also dispatches game-recovery operations
  with a separate tag: RECOVERY for checkpoints, CONTROL entry 2 for the cold
  resume probe/stage, preserving the SAVELOAD and CONFIG entries.
- `asm/overlay/recovery/`: game recovery (`docs/game-recovery-experiment.md`).
  `rcv_store.asm` owns the two alternating SRC2 checkpoint files (generation,
  CRC, journal link, flags); `entry_recovery.asm` writes and verifies them;
  `rcv_resume.asm` (linked into CONTROL) probes the newest offerable
  checkpoint and stages its linked `SHATJRNL.TMP` prefix into
  `SHATJRNL.NEW`. `src/spectrum/overlay/recovery_ovl.c` encodes the shared
  wire snapshot for checkpoints and manual saves with the RESTORE encoder in
  build-only mode. Probing never changes files or journal state; staging only
  writes NEW and an unarmed `pend`, which the accepted apply consumes.
- `asm/overlay/journal/`: shared STJ1 codec `stj_codec.asm` plus the Classic
  `entry_journal.asm` wrapper. Classic links the codec in the JOURNAL overlay;
  Next links it in SAVELOAD entries 0/1 through `stj_saveload.asm`
  (`STJ_SAVELOAD=1`). The persist append is
  `asm/overlay/saveload/journal_append.asm` (SAVELOAD entry 3, linked by
  `JOURNAL_HOOK=1`); resident state and hooks live in
  `asm/spectrum/journal.asm`. Packaging, hook and codec are independent build
  capabilities.
- `src/spectrum/session/`: compact production DIRECT/MQTT FSMs, MQTT wire
  adaptation, and ordered connection actions for ZX/Next. The cold MQTT action
  units are compiled into `MQTT_CONNECT` and `MQTT_TX`; they call only local
  transport primitives, so executing an action never nests overlays. Session
  code owns the Z80 decisions and shared transcripts judge them against the
  canonical reducers. CHAT prefixes take a resident fast path because the next
  operation renders through GUI_LOG; other game payloads retain the cold
  CONTROL classifier. Classic installs a resident IM2 frame ISR after loading
  assets, using the guarded `0xfcff..0xfd00` vector pointer to keep the low 16
  bits of `FRAMES` moving while DivMMC owns the ROM. Next retains ROM IM1.
  DIRECT receive, GUI_LOG chat rendering, FILEUI directory work and Classic
  About enable interrupts around their longer work. Classic bounds file reads
  to 255 bytes to reduce mapper/syscall overhead while retaining UART pumping.
  Every shared live-interrupt call closure preserves the SDCC/IY invariant
  `IY=0x5c3a`; SpectraNext retains its separate interrupt policy.
- `src/spectrum/session/identity.*`: resident role, transport, seating and SID,
  plus the bootstrap operation used by CONFIG. The matching
  `asm/session/identity_setup.inc` owns the normalized Setup handoff while
  remaining physically inside TIME_CONFIG, with the existing entry ABI.
- `src/spectrum/session/state.*`: persistent ply, pending MOVE/START/control/
  RESIGN/TAKEBACK/RESTORE bookkeeping and seat-probe state. Callers read a const
  view and use explicit write operations. The session schedule type retains
  loop-local retry/reannounce counters on the stack, not in new resident BSS.
  The phase-4 extraction places transition decisions in `session/transitions.inc`,
  composed into the application translation unit through `app/session_ports.h`.
  These compile-time semantic ports keep board, presentation and navigation
  effects in the application without a runtime callback table or action queue.
  Ownership means operation and storage definitions, not linker-enforced private
  writers: macros execute in the consuming translation unit. Guards are lexical;
  each semantic port has an explicit reviewed private-reference allowance, and
  new private dependencies fail the guard. Bodies still require policy review.
  Request macros take explicit
  caller-owned exit labels; guards reject hidden return statements.
  Shared parity, ownership guards and the Classic/Next ABI/size gate validate
  this boundary; measured closure is recorded in `session-core-contract.md`.
- `src/spectrum/board/`: Spectrum board state and parsed move application. Host
  tests use the compact C rules reference; shipped targets dispatch the ASM
  rules engine. Board code may dispatch cold board/rules overlays only
  through `spectrum/overlay/overlay.h`; overlay internals stay private.
- `src/spectrum/ui/`: GUI state, status bar, timer presentation, chat/move
  rendering, and input interaction. `cell_cursor.c` owns one private logical
  cursor and calls the compile-selected `space_navigation.h` operation;
  `chess_navigation.c` supplies Shatranj's geometry/orientation binding.
  `board_interaction.c` owns its private selection and chess origin/destination
  choreography. Together the owners retain four bytes of persistent state.
  `interaction.h` exposes operations, not writable storage. Application admission
  precedes activation; `app/ux_ports.h` declares borrowed fact queries and
  synchronous move/hint intentions, implemented by application coordination.
  Interaction exchanges row/column coordinates and selectable-cell facts, not
  chess characters or move strings. Application adapters own square labels,
  selection messages, automatic queen promotion and preferred cursor placement;
  the existing session/board path retains legality and peer-acceptance semantics.
  Host tests link the same module separately and drive its public operations.
  Dependency guards supplement compiler-enforced storage privacy.
  `chess_move_view.inc` owns chess move repaint/animation consequences, composed
  into `gui.c` to retain private view access, optimized kernels and existing ABI.
  `asm/spectrum/editor_state.asm` owns the five editor bytes as local symbols,
  in their existing ABI order, plus the resident append-at-end path used by
  normal typing. INPUT_EDIT receives their map-based imports for cursor,
  deletion and history operations; `ui/editor.h` publishes that hot operation
  and a volatile read-only activity view for application dispatch: zero means
  inactive, any nonzero mode means active. INPUT_EDIT remains the cold
  editor/command-recall owner and
  FILEUI retains resident navigation; neither requires another runtime layer.
  The menu retains FILE, FLIP and ABOUT as direct tabs. NTWRK, OFFER and THEME
  are driven by the resident table controller in `asm/spectrum/taboption.asm`.
  Its root descriptors and the fixed-width label/transition/action asset
  tables define direct tabs and rolling groups; the controller returns action
  ids without owning application policy. One phase runs per frame with a UART
  pump between phases. Q/A or the up/down arrows select their child actions,
  which reuse the existing session, board-theme, piece-set and movement-hint
  operations without another overlay dispatch.
  `ui/setup.c` owns Setup composition, navigation dispatch, focus acceptance,
  edit/action feedback and reconstruction after SAVE. Application effects are
  declared in `app/setup_ports.h`: session start, config persistence, clock
  updates and piece loading. `ui/setup.h` exposes a borrowed read-only view;
  `asm/spectrum/setup_state.inc` binds its private aliases to the existing
  CONFIG/RESTORE arena and owns the resident write operations. Only MENU_CONFIG,
  SETUP, INPUT_EDIT and TIME_CONFIG import those private overlay addresses.
  The arena is not a second UI allocation: CONFIG invalidates it while saving,
  and Setup rebuilds before rendering or reading its next selection.
  The UX ASM guard derives offsets from the C declaration and compares the arena
  aliases in field order; that ordering is part of the guard contract even when
  historical ASM names differ from C field names.
  Product GUI reads the board only through the
  immutable fixed-low-RAM view owned by `ui/gui.c`; it must not include board
  internals,
  mutate that storage, or cache board-owned pointers. Host judges replace that
  view with a copied snapshot through the test-only GUI adapter.
  Board-piece visibility is queried through its getter and changed through
  GUI operations. The raw byte retains its GUI_LOG/ASM read ABI; the UX guard
  rejects writes and address borrowing outside `gui.c`.
  GUI_LOG is a reviewed scalar reader independent of line formatting; its
  declaration and the resident definition are checked against the `uint8_t` ABI.
- `src/spectrum/transport/`: app-facing link facade, ESP AT networking, raw TCP
  bridge mode, MQTT packet transport, direct keepalive grammar, and link-level
  buffers. Session code supplies MQTT route and retention; transport does not
  classify Shatranj payloads. Text protocol parse/build belongs in
  `src/common/protocol/`. It may
  dispatch cold network overlays only through `spectrum/overlay/overlay.h`;
  overlay internals stay private.
  `net.h` imports no configuration. `net.c` explicitly imports session identity
  for the read-only transport-mode accessor and `session/timing.h` for polling;
  these are its two allowlisted session dependencies. ESP AT prefix matching
  uses the neutral `common/text.h` declaration. Only `keepalive_protocol.c` may
  directly include `game_protocol.h` for the shared DIRECT PING tokens. Classic
  and Next pop already-queued DIRECT payloads in the resident transport facade,
  leaving GUI_LOG cached across each queued part of an incoming chat burst.
- `src/spectrum/platform/`: Spectrum platform wrappers for frame wait, UART,
  text append helpers, and cold-path runtime hooks used by networking.
  Application loops consume `spectrum_input_poll_event()` directly; GUI has
  no physical-key polling facade. Move-command parsing belongs to `app/input_cmd.h`.
- `src/spectrum/lowram_map.h`: fixed low-RAM addresses and sizes for resident
  rings, logs, status, board/context storage, frame timing, and overlay scratch,
  with compile-time overlap checks. GUI notice flags/lifetime, last ply and log
  counts occupy `0x5ff8..0x5ffe`; Setup entry initializes them through
  `spectrum_gui_reset_logs()` and the initial empty notice before first use.
  Static checks cover Setup/TIME_CONFIG/menu context aliases and both target
  branches of the timer-roll buffer; linked checks also validate MQTT/RULES/GUI
  scratch aliases against the selected target map.
- `src/spectrum/render_status.h`: narrow declaration surface for the resident
  error-status renderer implemented by `asm/spectrum/screen.asm`.
- `src/spectrum/overlay/`: C bridge for cold overlays. UI includes are
  allowlisted: cold overlays may use `ui/layout.h` for coordinates,
  `overlay_api.h` may expose the minimal info-panel ABI, and `overlay.c` may use
  `ui/gui.h` as resident dispatch glue.
  Application-header dependencies are limited to
  `overlay.c -> app/input_cmd.h`, which declares its resident input-edit dispatch
  operations, and `recovery_ovl.c -> app/time_view.h` for read-only clock data.
  Other overlay-to-application includes fail the layering guard.
  SAVELOAD's public header exposes only file operations. Application RTC probes
  reuse the Setup TIME_CONFIG initializer; SpectraNext BUSY formatting uses a
  session operation implemented in this resident dispatcher. The guard rejects
  raw overlay ids/context aliases/exec in application code even through indirect
  includes. The sole context-read exception is `app.c` passing Setup RTC result
  bytes 2, 1, 0 directly to `spectrum_net_runtime_set_clock`; other offsets,
  consumers, address borrowing and mutations remain forbidden.
  STATUS reads endpoint IP through `transport/link.h`; overlays cannot import
  the ESP-AT header.
  Classic, Next and SpectraNext reject recursive dispatch before modifying the
  active overlay or loader state. A production Classic dispatcher vector covers
  both entry points, invalid entries, load failure and subsequent recovery.

## Desktop Code

- `src/pc/client/`: the single Qt implementation for Windows, macOS, and Linux.
  `DesktopSessionController` owns one active `DesktopSessionAdapter` and the
  session timers; `DesktopTransportCodec` owns TCP/MQTT framing;
  `SaveGameStore` owns desktop persistence. `main.cpp` is the executable
  entrypoint; `main_window.cpp`,
  `AppBanner`, and `PieceRenderer` are the Widgets UI. `BoardWidget` owns visual
  selection, orientation, board composition and animation resources;
  `CommandInputWidget` owns editing, recall navigation and submission gestures.
  MainWindow supplies position/legal targets, admits intentions and retains
  semantic `MoveRecord` history, session operations, promotion and persistence.
- `client/`: cross-platform desktop build wrappers, CMake targets, resources,
  and packaging metadata only. Its concrete `shatranj-client-ui` target compiles
  the Widgets implementation once for the application and integration test;
  OS resources remain attached only to the executable.

CMake enforces the dependency direction:

```text
shatranj-common (portable C, no Qt)
    -> shatranj-desktop-core (Qt Core/Network, no Widgets)
        -> shatranj-client-ui (Qt Widgets/Svg)
            -> shatranj-client (entrypoint and OS packaging)
```

## Assembly

- `asm/spectrum/classic_boot.asm`: one-shot Classic asset loading, IM2/tick
  initialization and fatal DAT rendering in the reusable overlay slot at
  `0x6800`. `classic_boot_refs.asm` preserves their resident linker names.
  `tools/package_classic_boot.py` imports the final resident map and assembles
  the capsule; its routines must finish before the first CONFIG dispatch.
- `asm/spectrum/screen.asm`: Classic/SpectraNext composition root, selecting
  its board/hint view and shared ULA primitives.
- `asm/next/screen.asm`: Next composition root, assembled as resident stubs
  plus state, and separately inside the extension. Its board/hint view and
  resources are Next-owned; it does not include the Classic root.
- `asm/ula/`: reusable ULA text, panel, address and drawing primitives. Each
  root selects these includes directly; no dispatch table is introduced.
- `src/spectrum/ui/layout_{zx,next}.h`: target visual values. `gen_ui_layout.py`
  emits the selected `ui_layout.inc` for renderers and MENU_CONFIG/TIME_CONFIG.
  FILEUI and GUI use the same C definitions. `log_contract.h` owns fixed record
  capacities; layout changes do not redefine session history storage.
- `src/spectrum/ui/presentation_{zx,next}.h`: direct-call expansion for About,
  board hiding and theme repainting, selected by `presentation.h`. GUI state
  and orchestration remain shared; SpectraNext placement gates stay explicit.
- `asm/app/`: hint request/coordination fragments, composed at their original
  resident or RULES-overlay placement. This owner may call the rules engine;
  render kernels may not. `screen_dispatch.inc` retains the existing resident
  input/setup/overlay glue extracted from the screen composition.
- `asm/uart/`: DIVMMC/divTIESUS UART backend.
- `asm/esxdos/`: esxDOS file/overlay loader code.
- `asm/platform/`: small platform primitives such as HALT and physical keyboard
  scanning/repeat/queue operations. Presentation ticking stays outside that
  input kernel.
- `asm/overlay/rules/`: production cold rules overlay and engine shipped by
  Classic, Next, and SpectraNext.

ChessZX is a visual/architectural reference for board/UI flow and piece asset
organization; do not vendor its source without a clear license.

## Assets

- `assets/spectrum/`: Spectrum-specific binary and ASM assets.
- `assets/next/`: Next sprite/About resources and presentation data. The
  maintained `ui_theme.asm` replaces slots in the existing UI DAT through
  `gen_assets.py`; `theme_constants.asm`, `coord_palette.asm`, `hint_inks.asm`
  and `piece_set_menu.asm` are included at their original resident/extension
  or MENU_CONFIG locations, only for Next. No new payload or runtime dispatch
  is introduced. `ui_runtime_assets.asm` owns the Next font and UI data;
  its offsets live in the Next composition, validated by the shared generator.
  See `next-graphics-contract.md` for placement, dependencies and verification.

## Tests

- `examples/space_demo/`: standalone rectangle 12x10 and hexagonal region with
  a hole, a filtered two-player view and single-cell reveal. Qt and Spectrum
  renderers share its model and the productive `cell_cursor.c`, without linking
  chess, session or transport. Build and controls: `game-space-contract.md`.
- `tests/common/`: logical-space bounds, connectivity and demonstrator model
  checks; `tests/tools/test_space_demo_z80.py` executes its linked Z80 renderer.
- `tests/rules/`: chess wrapper tests.
- `tests/net/`: MQTT, game-protocol, MQTT-session, and keepalive grammar host
  tests.
- `tests/session/`: shared DIRECT/MQTT transcripts, common judges, and canonical
  plus host-compiled Spectrum runners.
- `tools/stj_read.py`: host reader for Classic `.STJ` v1, 60-byte legacy
  snapshots, and headerless `SHATJRNL.TMP`. It does not run on the Spectrum.
- `tests/spectrum/`: Spectrum board/rules, config, and session host checks.
  Its ASM vector executes the production rules overlay and correlates it with
  `src/common/chess/rules_compact.c`.
- `tests/pc/`: desktop-core adapters, controller, persistence, framing, and
  failure-path tests.

## Boundaries

- Transport decides how bytes move, not whether a chess move is legal.
- Protocol defines payload grammar, not UI state.
- UI renders state and input feedback, not session negotiation rules.
- Operating-system differences belong in packaging and deployment unless an
  actual platform API requires a small adapter; there are no separate Windows,
  macOS, or Linux application forks.
- Overlays hold cold Spectrum logic that should not inflate resident 48K code.

# Hardware validation

Real-hardware results for Shatranj builds, newest first. Each entry records the
date, the build or commit, the machine and role, what was exercised, and the
result. Software and emulator gates never stand in for these results; missing
evidence stays pending (see [`maintenance.md`](maintenance.md)).

## 1.3 release

The results for the published 1.3 artifacts are recorded here when the release
smoke test runs. Record the cartridge firmware version and the artifact hashes.

Release smoke checklist (all pending until recorded):

1. Spectranext direct launch over HTTPS (Pages root) and installer
   (`install/`): start, About and the C changelog page, a piece-set change, a
   Direct and an MQTT game against Qt, FILE SAVE/LOAD, power cycle and
   relaunch with settings and saves intact; the installed copy still starts.
2. Classic and Next against Qt over MQTT (both roles): connect, player names
   in both headers, a chat burst that scrolls, FLIP, a declined and an accepted
   DRAW, statistics with S, and the host's restart.
3. Classic or Next: power cut mid-game and `Resume last game? Y` with the move
   list restored.
4. Qt with Stockfish against a Spectrum: `STOCKFISH` in the header,
   `VS <platform>: STOCKFISH`, and a takeback asked during the engine's pause.

### Release candidate `ef1ef0f`

- On 2026-10-13 a Spectranext cartridge started the final Spectranext build
  over HTTPS from the private test site
  (`https://ignaciomonge.github.io/netchesszx/`, Load Resource URL), showing
  the installer's title screen before the game. Result: PASS (operator
  report: "Montado por https funciona" and "Funciona con la pantalla de
  carga"). The guided installer from `.../netchesszx/install/` was checked
  too ("Instalación desde netchesszx/install comprobada"). Cartridge firmware:
  the latest stable release, the same that runs SpecTalkZX 1.4.2 (1.0 or
  later). The individual checklist items above were not reported separately.
  Payload SHA-256: TAP
  `4a90b2c319ef15fa5558103037d8cfade03dd79daa185b20f909be0ae74ab198`, OVL
  `a671571890511e1ba281043ce66faebb1f9d223079a0cadb2c0e4372195d3ea7`, DAT
  `43b481bd3f21e197652138ea6660d78d96b38c862d66d8556feef9ab550e303c`.
- On 2026-10-13 the Qt client built from `ef1ef0f` on Linux opened the manual
  (? / F1), and the Engine tab, the pause setting and the violet Stockfish
  button worked. Result: PASS (operator report: "Comprobado y funcionando").

### Spectranext direct launch

- On 2026-10-13 a Spectranext cartridge started the `make spectranext-direct`
  package of `e324e4b` from a LAN HTTP folder with **Load Resource URL**. The
  access log shows `HEAD /` and one `GET` each for `boot.zx`,
  `SHATRANJ.tap`, `SHATRANJ.DAT` and `SHATRANJ.OVL`; the game started on the
  first try. Result: PASS for HTTP. The cartridge firmware version was not
  recorded. Payload SHA-256: `boot.zx`
  `cd2813d204a409a4e9dc83cb4ba6b23309672600006453758b18be14a011c363`, TAP
  `9fe840428429b7b0bfedb8180590dd466f469737dcd37efee9dd8a85872f5766`, OVL
  `0ca8aa84f9c6da796b2c8eb0f6abd5798e3c7304becbce33189ed0155814e8d0`, DAT
  `950ee121e8cfd6af7c7cf509e53b17e8665e6428c457428fbfc3412a4b26a0f3`.
  HTTPS (GitHub Pages) is pending.

## Development snapshot 1.2.1 (2026-09-07 to 2026-10-13)

These notes were kept in the changelog's development section until the 1.3
release.

- On 2026-10-11 a real ZX Spectrum Next ran `7966ef2` as DIRECT host (white)
  against the macOS Qt client: FILE SAVE, FILE LOAD (restore accepted by the
  peer) and a power cut mid-game followed by `Resume last game? Y` restored
  the board and the full move list, with no "Recovery failed" notice.
  Result: PASS. Tested artifact: NEX
  `1DF7AE18258BEA897266ECF8FCBC385B53A1055C39B31A56E902B2C5D6481116`; Qt
  `shatranj-client`
  `543DB7A3105723FCE897575A41F9A5845C6ACE8579EF1A127C736B427E43F226`.
- On 2026-10-11 a real ZX Spectrum Classic ran `718ee3b` (the ACK of a move
  classified without loading CONTROL) through the same 16-ply DIRECT probe:
  17 plies, no PING lost, no retransmission; guest MOVE ACK latency median
  1.996 s, mean 1.988 s (one load less in the window: within the ±60 ms
  spread, consistent with about 30 ms per load). Result: PASS.
  Tested artifacts: TAP
  `5A89277CDD4D54399B501E0A36C0735C36101DABDE4C3DD2978904F54F1EBD27`, OVL
  `149EF78F0C477110DD0C911985D87F5442CCF914D8850021E55B1500E2829B6C`, DAT
  `23DAD24ACC6AB1FDFBE6B0956413CBAB57028406D2BE1DDAADAD293746D392E6`.
- On 2026-10-11 a real ZX Spectrum Classic ran the October optimization
  builds as DIRECT host (white, SAN). `tools/direct_burst_probe.py --lines 1
  --ping-every 0 --plies 16` measured the ACK latency of the guest's eight
  moves: `6d2ea35` median 2.058 s (mean 2.069 s), `3cd2097` median 1.987 s
  (mean 2.013 s); no PING lost, no retransmission. The probe's window holds
  the Spectrum's handling of the ACK of its own move and of the guest's
  MOVE, which lost two overlay loads between these builds (`a890fc1`'s
  second GUI_LOG, `aa0453c`'s CONTROL): one ~2 KB overlay read costs about
  30 ms on this Classic. With `3cd2097` against the
  Qt client: FILE SAVE, FILE LOAD (restore accepted by the peer) and a power
  cut mid-game followed by `Resume last game? Y` restored the board and the
  full move list, with no "Recovery failed" notice. Result: PASS.
  Tested artifacts: `3cd2097` TAP
  `8B64A8054124AD8240349A2FCB61E2CBE99CF6B6264617BF9BE5C47822CBE403`, OVL
  `95F38A76EC9143D53E885EB3496F999793F821426B32ACAB8CD9EAEFE8D2AB49`;
  `6d2ea35` TAP
  `53A7F114F1FF32A234056EBC02094C65945001D4A25A68E923D5F072B23D8B60`, OVL
  `3188563E7A98048093F568B170EC4C6D06A4584AD3B083404CA121C82FD4F4A9`; both
  DAT `23DAD24ACC6AB1FDFBE6B0956413CBAB57028406D2BE1DDAADAD293746D392E6`;
  macOS Qt `shatranj-client` (installed 2026-09-13)
  `543DB7A3105723FCE897575A41F9A5845C6ACE8579EF1A127C736B427E43F226`.
- On 2026-10-04 a real ZX Spectrum Classic played the Qt client in both
  directions (Spectrum host / Qt guest and Qt host / Spectrum guest) with
  `d9460659`: declining a DRAW opened no second prompt. Result: PASS.
  Tested artifacts: TAP SHA-256
  `E5BD833168939706DB3495635140C56C9A8198D79F1A3B5EF47FC74636FD08B9`, OVL
  `BC89DA71AF07FA4A6A3E596A11EA5DFC8E714E97791468F6A625D1AE1BA4136E`, DAT
  `23DAD24ACC6AB1FDFBE6B0956413CBAB57028406D2BE1DDAADAD293746D392E6`; Qt
  `shatranj-client.exe`
  `E867DC714F1D4454680AF320EA8A03BB43ABF1BB87D6DB2D74AA074C0129D454`.
- On 2026-10-03 a scripted DIRECT guest (`tools/direct_burst_probe.py`)
  flooded a Classic host with 1200-byte bursts (24 full chat lines, six PINGs
  and its own MOVE in one TCP write) around every ply. Reference build
  `0cf37b1e` (255-byte chunked overlay reads), three cycles: 21 of 50 PINGs
  unanswered (21 were sent inside bursts; the evidence does not say which
  ones were lost), the in-burst MOVE lost 3 of 3 and accepted about 2.05 s
  after a retransmission. Single-read build (`e7a24c04` plus `b9d25f48`),
  four cycles: 29 of 53 PINGs unanswered (30 inside bursts), MOVE lost 4 of
  4 and accepted about 1.78 s after the retransmission. The link never
  dropped.
  Result: PASS for "the single-read loader loses no more than the chunked
  one"; the per-ply time saving in normal play is not measured. The loss
  itself is explained by a no-game sweep on the single-read build
  (`docs/evidence/direct-burst-sweep-single-read.json`): a pure receive
  absorbs at least 8 full lines (389 bytes) with nothing lost, so CTS and
  the UART ring hold; a line is dropped only when the Spectrum is answering
  while more than three lines arrive, because the three-slot DIRECT queue
  discards the fourth by design (3 of 3 repeats; backlog #30). One earlier
  sweep run lost a 3-line burst once and the host expelled the silent guest;
  it did not recur in six further tries and stays unexplained.
  Tested artifacts (single-read): TAP SHA-256
  `3B43E4D9DA6AF681F016E22555DC1038510FBD12478DE6E55FEE7E27C94BE366`, OVL
  `ADE9C81AC7DFABF3DDFA4FE37CD982D8C0289F53AC0AB39EA1BF87D55808D284`, DAT
  `23DAD24ACC6AB1FDFBE6B0956413CBAB57028406D2BE1DDAADAD293746D392E6`.
  Evidence: `docs/evidence/direct-burst-baseline.json` and
  `docs/evidence/direct-burst-single-read.json`.

- The 2026-10-02 artifacts (`86b597b8`: end-of-game statistics, host-only
  restart, player names) were deployed to the Classic, Next, SpectraNext and
  Qt test machines; their SHA-256 fingerprints are in `docs/game-stats.md`.
  The statistics page and the restart flow were validated in the ZEsarUX/
  DIRECT harness as host and as guest. A two-machine physical restart,
  FILE SAVE/LOAD/ERASE with statistics, and mixed MQTT peers remain pending.
- On 2026-09-28 Classic (esxDOS) and Next (NEX) restored an interrupted game
  after a power cut; on 2026-09-29 SpectraNext installed with SPXI v2 and
  recovered the game after a reset. The move-list replay after recovery was
  added afterwards and is emulator-validated only; power cuts during a journal
  append or checkpoint write remain pending (`docs/game-recovery-handoff.md`).
- On 2026-09-30 the user confirmed that the single-socket build resolved
  SpectraNext DIRECT reconnection. Tested package SHA-256:
  `f915f7ebe4cb74a256ce92f3c403a5b7e1edbea4d204fca54968537bc04f4125`.
  The later removal of redundant driver ownership state passed host tests and
  eight FuseX Page B cycles; that simplified binary was not tested physically.
- On 2026-09-13 the user confirmed on Classic hardware that the resident
  append-at-end path removes the observed chat-input lag and associated timer
  loss. The Classic DIRECT path formerly swapped INPUT_EDIT with transport for
  every character; the exact transport and role used in the physical test were
  not specified. The tested diagnostic archive SHA-256 is
  `2D7990691D98A1D16CCC02FD8668B164B25DE8F54A0A36C6B082C593A8B95423`;
  hardware and ESP versions were not specified.
- On 2026-09-10 the user confirmed SpectraNext installation, configuration
  persistence after power cycling, MQTT/DIRECT play in both roles, RESTORE with
  timers, TAKEBACK, DRAW/new game and MQTT reconnection. The tested package hash
  and build evidence are recorded in `docs/session-core-contract.md`; precise
  TX-failure injection and dynamic stack high-water measurements remain pending.
- Initial real-hardware testing of UART commit `daa227f` was reported
  successful on both Classic and Next on 2026-09-07. Detailed scenarios and
  hardware/ESP versions were not specified; burst/CTS and injected-error
  recovery remain pending. The tested artifact hashes were recorded in an
  unversioned `build/uart_hardware_test.json` that is no longer present.
- Classic cold-start and reconnect after the startup-drain removal were
  confirmed on hardware by the user on 2026-09-07; the artifact hashes were
  recorded in an unversioned `build/uart_h1_hardware_test.json` that is no
  longer present.
- Spectranext Direct staging, MQTT stream recovery, esxDOS handle encoding,
  RTC apply-mode, and Next/SpectraNext paging are covered by host and
  production-ASM tests. They are not claimed as complete real-hardware
  validation in this snapshot.

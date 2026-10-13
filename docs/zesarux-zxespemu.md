# Local ZEsarUX and ZXESPEmu

## Prepared environment

The machine-local harness is:

```text
/Users/ignaciomongegarcia/Developer/Retro/ZXESPEmu
```

It is a separate Git repository and must be synchronized independently. Its
launcher, virtual ESP, tests, patches, and setup files are versioned there;
runtime logs and the downloaded/built emulator are intentionally ignored.

The reusable patched executable is:

```text
ZXESPEmu/vendor/src/ZEsarUX-13.0/zesarux
```

Normal runs reuse that local copy. The first run invokes the setup script only
when the vendor executable is absent; do not redownload ZEsarUX for each test.

## Run Shatranj

From the harness:

```sh
./run.py
./run.py --target classic
./run.py --headless
```

The registered Shatranj Classic and Next development launchers pass
`--netchess .`, so they open `release/SHATRANJ.tap` or
`release/Next/SHATRANJ.nex` directly from this checkout; they do not compile
during launch. A NEX written only below `build/` is a preview and is not visible
to those launchers. A generic ZXESPEmu run without `--netchess` instead opens
the latest complete prebuilt snapshot published under `ZXESPEmu/artifacts/`.

After generating TAP/OVL/DAT and NEX in any NetChessZX worktree, publish them
together for generic snapshot runs with:

```sh
/Users/ignaciomongegarcia/Developer/Retro/ZXESPEmu/stage_netchess.py "$PWD"
```

The update switches snapshots only after all four files have been copied, so
classic TAP/OVL/DAT cannot be mixed between builds. Use
`--netchess /path/to/NetChessZX` only for an explicit direct, non-staged run,
`--verbose` for every UART exchange, and `127.0.0.1` for a Direct desktop peer
on the same Mac.

Runtime state and logs are replaced safely under `ZXESPEmu/run/`; inspect
`launcher.log`, `zesarux.log`, and `zxesp.log` when startup or UART traffic
fails.

## Launcher lifecycle

Staging artifacts and publishing launchers are separate operations. Whenever
`zxespemu-launchers.json` changes, use the controller's live workflow from the
repository root:

```sh
ZXESPEMU=/Users/ignaciomongegarcia/Developer/Retro/ZXESPEmu
"$ZXESPEMU/launcherctl.py" contract
"$ZXESPEMU/launcherctl.py" validate ./zxespemu-launchers.json
"$ZXESPEMU/launcherctl.py" register ./zxespemu-launchers.json
"$ZXESPEMU/launcherctl.py" sync netchess
"$ZXESPEMU/launcherctl.py" status netchess
```

Completion requires every declared launcher to be `current` with no
duplicates. `sync` retires older managed versions; do not delete application
bundles manually.

## Virtual ESP scope

The virtual modem covers the commands Shatranj uses:

- Wi-Fi preflight and local IP reporting.
- TCP client/server, multiplexing, send, close, and incoming IPD frames.
- MQTT transparent binary passthrough and guarded escape.
- SNTP configuration and time query.

It does not emulate the ESP8266 CPU, radio, Wi-Fi scans, TLS, or UDP.

Run its focused tests with:

```sh
PYTHONPATH=. python3 tests/test_zxesp.py -v
```

## Per-ply overlay trace

`tools/emu_ply_trace.py run` boots this checkout's Classic `release/` with a
writable SD, sets it up as the DIRECT host, plays a scripted game against a
guest peer and prints the overlay loads and SD operations of every ply
(`analyze --out DIR` reprints a saved run). It counts loads and I/O, not SD
latency, and the emulator cannot create the journal file (open mode 0x0F).

## ZEsarUX patches

The local setup applies:

- `patches/zesarux-13-uartbridge.patch` for the required command-line UART
  bridge.
- `patches/zesarux-13-tbblue-ulaplus.patch` for Next ULA+ palette rendering.

Stock ZEsarUX 13.0 conflates TBBlue ULA+ enable (`NextReg 0x68` bit 3) with
ULANext enable (`NextReg 0x43` bit 0). Consequently Shatranj's valid private
attributes `0x81` and `0x8A` can appear blue with FLASH in themes 2-5. The patch
selects the ULA+ palette group from attribute bits 6-7. It was reported with
the patch at <https://github.com/chernandezba/zesarux/pull/12>.

This is an emulator correction, not a Shatranj wire or rendering workaround.
Real Next hardware remains authoritative for timing, MMU/IFF restoration, UART,
FPGA-core behavior, and final visual acceptance.

## Recorded emulator checkpoint — 2026-07-27

On 2026-07-27:

- The final chat/control tree passed `make test` (MQTT parity 56/56 and DIRECT
  parity classic and Next) and rebuilt both targets with `make tap nex`.
- The Spotlight snapshot was published atomically from that tree. Its NEX is
  82,432 bytes with SHA-256
  `41538919f5901393dc5491bba8eac27f6d0852d4b47cd5e2ce4de72248eeba81`;
  `ZXESPEmu/artifacts/netchess-current/manifest.json` records the matching
  classic TAP/OVL/DAT hashes.
- `make client-test` passed 8/8 and the matching signed Qt bundle was installed
  at `/Applications/Shatranj.app`.
- Ignacio completed interactive acceptance in ZEsarUX/ZXESPEmu for the classic
  ZX and Next builds, including the recent palette, About, chat, DRAW, and
  RESIGN changes. Physical hardware was not run; by owner decision this
  emulator evidence closes the release blocker for all commits through this
  checkpoint.
- Commit `767404a` fixed the Qt connected-window shutdown crash and added its
  regression test.

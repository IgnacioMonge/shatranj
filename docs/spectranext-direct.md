# Direct Spectranext HTTP(S) Launch

Launches the Spectranext cartridge edition from an HTTP(S) resource folder
without installing it to local XFS. The installer flavour
(`packaging/spectranext/installer.json`, `make spectranext-port-build`) is
unchanged and remains the way to install.

## Build

```sh
make spectranext-direct SPXN_DIR=/path/to/SpectraNext/driver
```

This builds the Spectranext TAP (`make tap-spectranext`), writes the compact
tape launcher, stages `packaging/spectranext/direct.json` into
`build/spectranext-direct/` and runs `tests/tools/test_spectranext_direct.py`.
The previous folder is replaced atomically (stage, then rename) and kept if
staging fails. The Spectranext test suite is not run here;
`spectranext-port-build` owns it. Building does not publish anything.

The folder contains `boot.zx`, `SHATRANJ.tap`, `SHATRANJ.OVL`, `SHATRANJ.DAT`,
plus the SDK's `index.txt`, `index.html`, `package.json` and `.nojekyll`. The
payloads are byte-identical to `release/Spectranext/`. `boot.zx` is a BASIC autostart (line 10) whose only
statement is `%tapein"SHATRANJ.tap":` followed by tokenized `LOAD ""`.
No `%fs`, no installer and no copy to local XFS run.

The Spectranext TAP (`tools/package_spectranext_tap.py`) loads its CODE, then
the installer's picture (`assets/spectrum/installer.scr`) as SCREEN$, pauses
two seconds and starts, as SpecTalkZX does; the installed copy uses the same
TAP.

## Publication

- Publish all files of one build together, in one deployment.
- Never mix TAP, OVL and DAT from different builds.

Releases publish the folder at the root of the public GitHub Pages site,
`https://ignaciomonge.github.io/shatranj/`, with the landing page as its
`index.html`; the guided installer lives under `install/` (the SpecTalkZX
layout). `make spectranext-pages SPXN_DIR=...` assembles that tree in
`build/spectranext-pages/`; publish it as one Pages deployment. Pages serves
each file with `Cache-Control: max-age=600`, so for up to ten minutes after an
update a start may combine cached and new files. The OVL atlas fingerprint
rejects an OVL that does not match the TAP; a stale DAT beside a new TAP is
not detected. This residual risk is accepted, as in SpecTalkZX.

Before a release date is known, keep the changes under `Unreleased`, set
`VERSION` to the candidate version, and stage locally without a dated entry:

```sh
make spectranext-port-build spectranext-direct SPXN_DIR=/path/to/SpectraNext/driver
python3 tools/build_spectranext_pages.py --preview-codename "Open Match"
python3 tests/tools/test_spectranext_pages.py
```

This only creates local files. Once publication is approved, date the release
in the changelog and run the Pages builder without `--preview-codename`;
it checks that the release heading matches `VERSION`. Existing matching
TAP/OVL/DAT and installer resources need no rebuild for that metadata change.

## Firmware

Startup asks the cartridge for its build version (`$3EF0` operation 15, added
by firmware 1.0, which also fixes seeking in files read over HTTPS). Older
firmware returns carry: startup stops with a red border and
`FIRMWARE OUTDATED: SEE README`. Both manifests declare
`minimum_firmware` 1.0.

MATCH additionally needs firmware 1.2 or later. On older supported firmware,
the game offers DIRECT and MQTT; the resource minimum remains 1.0.

## Launch

- Load Resource URL with the folder URL (HTTP or HTTPS).
- LAN HTTP mount: `%fs N`, then `%load "boot.zx"`.

## Resource lifetime

- `SHATRANJ.DAT` is opened once on the inherited launch mount and its
  descriptor is retained for the session. About and piece-set changes re-read
  through it.
- `SHATRANJ.OVL` is opened on the same mount, copied once into cartridge SRAM
  pages at startup, then closed.
- If the launch mount lacks the files, the loader falls back to local slot 0
  (installed package).
- Configuration, saved games, journal and recovery stay on local XFS slot 0,
  unchanged.
- The source server is only needed at launch.

## Cartridge validation

Check a new package on the cartridge with these steps:

1. LAN HTTP launch (`%fs N`, `%load "boot.zx"`) with no local DAT/OVL.
2. Stop the server after startup, then check About, a piece-set change, a
   networked game, save/load, a power cycle and relaunch.
3. HTTPS launch via Load Resource URL.
4. Confirm the installed package still launches.
5. Check the 4-handle VFS budget (retained DAT, config/save handles, tape).

# Building Shatranj

[Español](README.es.md) · [User guide](../README.md) · [Qt client guide](../client/README.md)

The repository includes the shared game code, all four client targets, assets,
packaging tools and tests. Download ready-to-run packages from the
[latest release](https://github.com/IgnacioMonge/shatranj/releases/latest).

## Requirements

- GNU Make, a POSIX shell and a C99 compiler for the host tests.
- Python 3.10 or later with Pillow for asset generation and tool tests.
- z88dk `v24860-762578732` for Spectrum builds. CI pins the matching toolchain.
- Qt 6, CMake and a supported C++ compiler for desktop builds; see the
  [platform instructions](../client/README.md).
- The SpectraNext driver SDK for cartridge builds, selected with `SPXN_DIR`.

## Build and test

Run these targets from the repository root:

```sh
make test          # Host tests and Z80 vectors
make client-test   # Build and test the Qt client
make tap           # Classic: release/SHATRANJ.tap, .OVL and .DAT
make nex           # Next: self-contained NEX
make client        # Package the desktop client
make full-check    # Host tests, module boundaries, ABI and size limits
```

`full-check` already rebuilds Classic and Next. For release packages, use
`make release-check`: the same gate with the release compiler optimization
budget. Keep the Classic TAP, OVL and DAT from the same build together.

Spectranext installer and HTTP(S) packages:

```sh
make spectranext-port-build spectranext-direct SPXN_DIR=/path/to/SpectraNext/driver
make spectranext-pages SPXN_DIR=/path/to/SpectraNext/driver
```

See [Spectranext HTTP(S) launch](spectranext-direct.md) for resource layout and
firmware requirements. `PORT`, `MQTT_HOST`, `MQTT_PORT` and `MQTT_CODE` may be
passed to Make when building preconfigured Spectrum packages.

## Source map

| Directory | Contents |
| --- | --- |
| `src/common/` | Shared chess, protocol, session and save-format code |
| `src/spectrum/`, `asm/` | Spectrum runtime, overlays and target-specific assembly |
| `src/pc/`, `client/` | Qt application and desktop build entry points |
| `assets/` | Game graphics, fonts and icons |
| `tools/`, `packaging/` | Asset generation, validation and release packaging |
| `tests/`, `examples/` | Regression tests, fixtures and the shared rendering example |

## Reference

- [Wire protocol](wire-contract.md): Direct TCP and MQTT messages and compatibility.
- [ABI checks](abi-guard.md): Classic and Next overlay contracts.
- [Size checks](size-report.md): memory budgets and baseline comparison.
- [1.4 Open Match](releases/1.4.md): release notes.
- [Changelog](../CHANGELOG.md): released and upcoming changes.

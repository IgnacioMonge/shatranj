# Shatranj Architecture Decisions

## 0001 - Project Name

Decision: visible product name is Shatranj.

Reason: Shatranj is the visible product name; protocol/internal prefixes stay
stable for compatibility.

Current generated names:

- Spectrum TAP: `SHATRANJ.tap`
- PC client: `shatranj-client.exe`
- Protocol prefix: `netchesszx/v1`

## 0002 - Chess Core Layout

Decision: keep chess rules owned by Shatranj, not vendored around `mcu-max`.

Use this layout instead:

```text
src/
  common/
    chess/
    mqtt/
    protocol/
  pc/
    client/
  spectrum/
    app/
    board/
    overlay/
    platform/
    transport/
    ui/
asm/
  spectrum/
  uart/
  esxdos/
  platform/
  overlay/rules/
client/
tests/
```

Reason: Shatranj has separate responsibilities: Spectrum transport, PC GUI,
protocol, build outputs, and MQTT. The old vendored `mcu-max` copy is no longer
used by the build; keeping it would only preserve dead source.

Current contract:

- The vendored `mcu-max` copy was removed after the project stopped
  consuming it directly.
- `src/common/chess`: Shatranj-owned rules, position, and coordinate APIs.
- `src/common/chess/rules_compact.c`: desktop and host-test reference rules implementation; it is not linked into shipped Spectrum targets.
- `asm/overlay/rules/rules_stub.asm`: production rules engine shipped by Classic, Next, and SpectraNext.
- `tests/tools/test_spectrum_asm_vectors.py`: executes the production ASM and correlates its PLAY/CHECK results with the C reference.
- `tests/rules`: perft and special-rule tests against the owned rules layer.
- `src/pc/client`: consumes the owned common/Spectrum-compatible rules behavior.
- `docs/source-layout.md`: canonical source map for current paths.

## 0003 - One Cross-Platform Desktop Client

Decision: Windows, macOS, and Linux use one Qt client and one shared desktop
core. They are build/package variants, not independent applications.

Dependency direction is enforced with separate CMake targets:

```text
shatranj-common -> shatranj-desktop-core -> shatranj-client-ui -> shatranj-client
```

- `shatranj-common` is portable C and has no Qt dependency.
- `shatranj-desktop-core` contains chess helpers, session adapters and
  controller, transport framing, and save-game persistence. It uses Qt Core
  and Network but not Widgets.
- `shatranj-client-ui` compiles the Widgets implementation once for the app and
  integration tests; it consumes the core.
- `shatranj-client` contains the entrypoint and platform packaging resources.

Reason: a single implementation prevents behavior drift between desktop
platforms while target boundaries catch accidental UI/platform dependencies in
the shared core at compile time. Platform-specific code is added only when a
real OS API requires it; speculative interface hierarchies are rejected.

## 0004 - Spectrum Renderer

Decision: Spectrum screen rendering starts in ASM.

Reason: the C `printf` board was useful for transport proof but too slow and too
large. Screen clear, board drawing, cursor, attributes, and later piece blits
need direct VRAM writes from the beginning.

Boundary:

- C keeps protocol/network orchestration while it remains cheap enough.
- ASM owns rendering hot paths.
- ChessZX is a reference for UI/piece organization, not vendored code.

## 0005 - Spectrum Cold Overlays

Decision: keep the Spectrum resident core 48K-first and add a Spectalk-style
cold overlay file, `SHATRANJ.OVL`.

Reason: legal move validation, storage, options, and reconnect/resume logic are
cold paths. Keeping them resident would force a premature 128K-only boundary and
make later memory recovery harder.

Current contract:

- Resident owns UI, board drawing, network/protocol, timers, and turn state.
- Classic loads fixed 2048-byte blocks from `SHATRANJ.OVL` at `0x6800`.
- Spectranext executes overlays from dedicated 4096-byte cartridge SRAM pages
  mapped at `0x2000`.
- The Next NEX links resident code from `0x7000`. Each dedicated 8192-byte MMU
  page mapped at `0x6000` contains at most 4096 bytes of overlay code in its
  lower half and an immutable copy of resident `0x7000..0x7fff` in its upper
  half. This keeps that resident window executable while slot 3 is paged.
- The Next release gate requires at least 3072 bytes between linked BSS and SP,
  and at least 512 unused bytes in the permanent extension page.
- Overlay nesting remains forbidden on every target.
- Overlay 0 is reserved for chess rules.
- Local Spectrum moves are not applied locally until the PC returns `ACK <ply>`.
  If the PC rejects a move, it sends `NACK <ply> ...`; the Spectrum keeps the
  turn and board state.

This gives a safe protocol guard immediately while the full rules overlay is
filled in.


## 0006 - One Semantics, Two Implementations, One Judge

Decision: the portable reducers are the canonical PC/reference implementation,
while Spectrum and Next retain compact state machines optimized for their memory
budget. Both implementations must satisfy the same transcript corpus and wire
contract.

Reason: linking the generic reducer into Spectrum was measured at roughly
31 KiB of additional resident code and is not a viable way to obtain parity.
Shared judges enforce behavior without imposing the same runtime layout.
Target-specific expected results are forbidden: disagreement means one
implementation or the contract is wrong. Transport and UI adapters translate
events and execute actions; they do not decide session policy.

## 0007 - Next Keeps ULA+ Enabled

Decision: initialize the standard ULA+ palette and enable ULA+ once during the
Next graphics-bank bootstrap. Do not toggle ULA+ when the board theme or setup
screen changes.

Palette groups 0/1 mirror the classic ULA normal/bright colours, so the default
theme and the manually animated move flash keep their appearance. Themes 2-5
use private group-2 coordinate and frame colours.

Reason: switching palette interpretation independently in setup and theme code
allowed the attribute file to be rendered with stale state. One application-
wide mode removes that failure path and repeated resident/overlay NextReg work.

## 0008 - Either Peer May Offer Restore

Decision: Host and Guest may both load a save and offer it. The wire sequence
stays `RQ` / `RY`/`RN` / `RS00` / `RS01` / `RA`. The save's `host_color` must
match the current seating; Shatranj never rotates or colour-inverts a position.

Reason: restore is a negotiated snapshot, not a host privilege. The old gates
automatically refused a Guest offer. Removing them preserves the existing wire
and save validation, while avoiding a chess-state transform involving pieces,
turn, castling rights, en-passant and presentation. Older host-only peers still
refuse a Guest `RQ` without mutating the board.


## 0009 - Connection Setup Reveals Rows Sequentially

Decision: the Setup menu keeps revealing a row only once the row before it is
defined. Showing every row from the start is rejected. ROOM in particular is
never drawn before LINK has been chosen.

Reason: tried on 2026-08-30 and rejected at the emulator. With nothing defined
yet, a fully drawn menu reads as though every option were already selected,
while SAVE and START are correctly absent because nothing is configured. The
result misleads rather than simplifies. The sequential reveal is what tells the
user what still needs a decision, and the ACTION row appearing is what signals
the configuration is complete.

The machinery it costs is real: `su_compute_visible` builds the mask from the
defined mask, and collapsing it to a constant recovered 77 bytes of the SETUP
overlay. That saving does not buy back the lost affordance. Any future attempt
to reclaim those bytes must keep the reveal order intact.

# End-of-game statistics

## Behavior

Checkmate, stalemate, resignation and agreed draw leave the game in OVER.
There is no automatic rematch. Both players retain the board, moves and chat,
and only the host (the player who created the game) asks for a new one, so
two requests never cross between peers of this version. Spectrum shows the
result first; the next key shows the host `S:STATS  RESTART? Y/N` and the
guest `S:STATS  WAITING OPPONENT`. S opens statistics; any key (Y/N included,
so a stray key never restarts or leaves) closes the page, restores the board
and returns to the prompt. ENTER, on the board or on a prompt, opens chat as
during play; the next ordinary key re-shows the prompt. The host's request
reaches the guest as `S:STATS ACCEPT RESTART? Y/N`; S still opens the
statistics, and closing the page re-shows the request. Declining a new game
ends the session for both players. A 1.2 peer may still send RESET after
an agreed DRAW, as host or guest; it is answered with the same prompt.
Qt follows the same rule: Restart Game is enabled for the host only, the
guest's status shows that it is waiting for the host, and Game stats stays
available except while the host's own request awaits its answer.

### Restart after the game (Spectrum)

A new game starts only with an ACK, on both sides at once. A refusal ends the
session; an expired request leaves both players in OVER.

| Player | State | Event | Result |
| --- | --- | --- | --- |
| Host | OVER | any key except ENTER | `S:STATS  RESTART? Y/N` |
| Host | offer | S / ENTER | statistics / chat; the offer stays |
| Host | offer | Y | sends RESET; `WAITING ACK` |
| Host | offer | N or BREAK | leaves to Setup |
| Host | waiting | ACK RESET | new game on both |
| Host | waiting | NACK RESET | `RESET rejected`, leaves to Setup |
| Host | waiting | BREAK | leaves to Setup |
| Host | waiting | other keys (S included) | `WAITING ACK`; ENTER still opens chat |
| Host | waiting | no answer | 5 retries, 300 s grace, automatic CANCEL; offer again |
| Guest | OVER | any key except ENTER | `S:STATS  WAITING OPPONENT` |
| Guest | waiting | S | statistics |
| Guest | waiting | BREAK | leaves to Setup |
| Guest | host's request | Y | ACK; new game on both |
| Guest | host's request | N or BREAK | NACK, leaves to Setup |
| Guest | host's request | S | statistics; the request stays and is re-shown on closing |
| Guest | host's request | its CANCEL RESET (expired) | NACK; back to waiting |
| Either | a prompt is up | duplicate RESET | ignored (no reply, no restart) |

Leaving sends BYE: the other player sees the opponent leave. A RESET from a
1.2 peer reaches the host's offer as `S:STATS ACCEPT RESTART? Y/N`, and one
that crosses the host's own request is an agreement (ACK, new game on both).
Closing the statistics page never answers a prompt. Qt applies the same rule
to its controls: a declined restart, given or received, disconnects it; while
its own confirmation dialog is open it refuses an incoming request (NACK).

Statistics concern only the current game, are not saved or transmitted, and
are calculated from each client's observed TURN timer. Network/ACK delay can
therefore produce small differences between peers. The Spectrum restarts its
timer as soon as a move is captured, before it draws and checkpoints it, so
that processing time counts for the player now on the clock, as it does on
the peer; restarting it after that work left about three seconds per move
uncounted (25 s against the Qt peer's 34 s over three moves on 48K hardware).
The Qt client shows the same page during a game too, from its bar-chart
toggle beside **Log**. Only committed moves count:
the unfinished turn at resignation or agreed draw is excluded. No extra game
duration, move-count, check-count or event-count metrics are displayed.

## Metrics and capture semantics

- TIME: sum of completed TURN durations by chess colour, in whole seconds.
- AVG: integer floor of TIME divided by that colour's committed moves since
  the statistics base ply. An empty sample displays `--`.
- TIME %: each colour's rounded share of both TIME totals, shown right of
  TIME (`58%` / `42%`, always summing to 100). It is omitted while no time was
  recorded or after a total saturated. The 4x4 font gained a `%` glyph
  (previously the same placeholder box as `#`).
- MAX and AT, MIN and AT: longest and shortest completed turn and their
  one-based full-move numbers (formerly labelled LONGEST and FASTEST; the
  4-character labels free the columns needed for aligned names). The side's first move sets both; ties
  retain the first occurrence, including a first move taking zero seconds.
- CAPTURED: actual enemy pieces removed by committed moves, grouped by the
  capturing colour. En passant counts a pawn; promotion alone captures nothing;
  a promoted piece later taken counts as its actual piece type. Kings never
  appear. Each colour's figure is the material it captured (pawn=1,
  knight/bishop=3, rook=5, queen=9), `0` when it captured nothing; it is not
  the engine's evaluation of the position. The former signed balance is gone.
- AT shows the bare move number (`23`); the row label already names it, and
  the 4x4 font renders `#` as an ambiguous box.
- TAKEBACK restores the preceding timing aggregate (total, both extremes and
  their move numbers) and removes that move's capture. It uses the existing single-move undo lifetime, with no new history.
- Applied LOAD, remote RESTORE and crash recovery start an empty PARTIAL
  sample at the restored ply. Historical elapsed times and captures are not
  reconstructed from a position or an untimed move journal. Failed, refused or
  merely staged restores leave the statistics unchanged.
- START or accepted RESET starts a complete empty sample. Opening/closing the
  page and duplicate protocol messages do not change the sample.

The compact representation saturates time at 65,535 seconds and move numbers
at 255 (shown `255+`, meaning move 255 or later). Saturated values are marked
rather than wrapped; a saturated total cannot yield an exact average. The page
supports hours and empty samples.

## Storage and ownership

The portable oracle lives in `src/common/chess/game_stats.{h,c}`. Spectrum
captures in SAVELOAD entry 3 before TURN resets, independently of journal I/O
success. Presentation stays in ABOUT (including the Next stub's page). No
statistics accumulator or renderer moves into resident code.

The complete block is **33 bytes** (24 before FASTEST) to preserve TAKEBACK
and exact captures:

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0, 8 | 8 each | White/black: LE16 total, LE16 longest + uint8 AT, LE16 fastest + uint8 AT |
| 16 | 2 | LE16 base ply after START/RESTORE |
| 18 | 1 | Partial (0x01), total saturation (0x02/0x04), undo-valid (0x20), lost sample (0x40) |
| 19 | 9 | Previous mover's 8-byte aggregate and previous flags |
| 28 | 5 | Pawn, knight, bishop, rook, queen capture counts; white low nibble, black high nibble |

An AT of 0 marks a side without moves; the former AT saturation flags are
replaced by the 255 sentinel. TIME % and material are derived on the page.

After OVER, the first two bytes of the no-longer-live undo backup hold the
resident result-text pointer. This adds no persistent storage. Journal I/O
warnings do not degrade statistics already captured before that I/O.
If SAVELOAD itself cannot be paged in, the sample and undo are invalidated;
the page shows an empty partial sample, and the next captured move starts a
new partial sample. A later storage failure does not invalidate captured data.

This occupies Classic and SpectraNext `0x65f5..0x6615`, immediately after
player names. Classic's transient MQTT packet/ABOUT input moved from `0x660d`
to `0x6616` (it now ends `0x67f2`, below the `0x6800` overlay slot and still
containing the fixed overlay scratch); SpectraNext's starts at `0x662b`.
Next follows its names at `0x3f46..0x3f66`.
The low-memory checker must prove non-overlap for all three targets. Capture
input reuses the existing move undo, including its en-passant evidence.

## Spectrum page

The FILE interior is 128x128 pixels: x=8..135, y=40..167 (rows 5–20,
columns 1–16). Moves/chat remain visible. The title uses the real 8x12 banner
font and 0x47/0x07 attributes. The inverted result strip follows the active
board theme on row 8, one blank row below the title (FILE's header rule is
cleared). On the row-10 strip each MOVES colour disc starts exactly at its
value column (x32 white, x84 black) and the name follows two half-columns
later; ten-character names end 4 px before the frame.
Six metric lines use the 4x4 ikkle font at 6-pixel pitch (y91..121): TIME
(with the share right-aligned), AVG, MAX, AT, MIN, AT. The yellow
CAPTURED row carries each side's material, right-aligned like the share.
8x8 capture miniatures derive from the current 16x16 game sprites and use
their dark-square masks on black paper, three rows of five per side starting
under the value columns (rows 17–19), covering all 15 captures in a
king-against-king finish. The footer row holds PARTIAL (when needed) and
`ANY KEY: BACK`. The 16x16 grouped alternative was rejected as too large;
light capture backgrounds were rejected for poor contrast.

`tools/make_game_stats_mockup.py` produces the approval PNG from the real
assets. The 8x8 black-background version, including the king-against-king
example, was approved on 2026-10-01 before Spectrum implementation; the
2026-10-02 revision (lower result bar, aligned names, FASTEST, TIME %,
per-side material) was requested by the user after hardware use. The
production ABOUT code is compared pixel-for-pixel with that mockup.
The entry reveal runs top-to-bottom. Closing restores
the board like FILE; Next sprites are hidden while the page is visible.

## Implementation ledger and acceptance

| Option | Evidence/cost | Decision and proof |
| --- | --- | --- |
| Remove automatic DRAW/RESIGN rematch | Existing common/compact FSM paths and Qt latch; reduces resident work | Do; shared transcripts for local/remote/crossed requests, duplicates, failures and subsequent consented RESET |
| Reuse SAVELOAD append hook | Runs after commit and before TURN reset | Do; oracle/Z80 vectors including I/O failure, START, RESTORE and TAKEBACK |
| Reuse native STJ erase path | Capture ASM measures 462 bytes, exceeding the original 60–100 estimate | Do; remove generic C erase from native builds, reuse Next/SpectraNext codec and retain a small Classic parser; 90 erase vectors preserve names, failures and `.DEL` behavior |
| Derive captures from missing board material | Cannot distinguish a captured queen replaced by promotion | Reject; exact packed counts cost 5 low-RAM bytes |
| Keep only 13 bytes | Cannot roll back longest/total after TAKEBACK | Reject as complete budget; 6-byte undo plus capture counts brings total to 24 bytes, zero resident BSS |
| Render in ABOUT with local scaled glyph | Existing cold page, no nested overlays or new resident text helper | Approved; linked ABI/size checks, framebuffer and sprite restoration |
| Persist or transmit metrics | Outside current-game-only scope | Reject; mark restored samples PARTIAL |

Existing baseline (8bcbea66 tree): Classic resident 35,676 bytes, ordinary BSS
ends at 0xfcf7 (8 bytes before the IM2 vector), SAVELOAD 1770/2048 and ABOUT
467/2048. These replace the handoff's older 11-byte resident margin. Expected
code budgets are estimates until measured at link time. Preserve stable ABI,
UART servicing, IY/interrupt rules, consent, game history and board restoration.

Batch edits before one `make full-check`; do not precede it with overlapping
firmware gates. Qt closes with `make client-test`. SpectraNext is explicitly
in scope and requires its own linked size evidence. Update size baselines only
from the pinned toolchain after reviewing actual costs. Emulator, physical
Classic/Next/SpectraNext and mixed-version 1.2 evidence are separate gates;
none is implied by host or linked tests. No hardware pass is recorded yet.

## Verification record — 2026-10-01, revised 2026-10-02

Developed in the (since removed) worktree `feature/usage-stats`, based on
`8bcbea66`, and integrated into `main` as `af59b31f` plus `86b597b8`
(host-only restart), using the pinned z88dk `24860-762578732c-20260630`.
Final linked measurements (2026-10-02):

| Target | Resident bytes | Ordinary/fixed BSS bytes | SP gap | SAVELOAD | ABOUT |
| --- | ---: | ---: | ---: | ---: | ---: |
| Classic | 35,676 | 411 + 2 | 599 | 1915/2048 | 2017/2048 |
| Next NEX | 32,892 | 511 | 3293 | 3787/4096 | 1557/4096 |
| SpectraNext | 36,037 | 262 | 645 | 4083/4096 | 2938/4096 |

Resident growth since the first record: HINTS ON/OFF notice (+11 bytes),
code-review fixes (+44: chat after the game, the peer RESET superseding the
local restart offer, re-showing a pending prompt when a page closes) and the
2026-10-02 revisions (+27: S on the opponent's restart request in OVER;
+173: the host-only restart — the guest's waiting notice with S, BREAK
leaving after the game, a refusal ending the session on both sides, and the
statistics page closed when a new game starts).
SpectraNext's SP gap falls to 645 bytes: above the
512-byte floor, below the 768-byte warning.
That revision's FASTEST capture costs no SAVELOAD space: the capture routine
was restructured (shared extreme update, inlined saturation flag, 255 AT
sentinel) from 462 to 453 bytes. ABOUT grows by 140 bytes for TIME %,
MIN/AT, per-side material, discs aligned with the value columns (any
sub-cell shift) and pixel-row metrics, which now
use the existing resident `spectrum_render_ikkle_abs_at` (ABI baselines
regenerated).

Classic saves **173 resident bytes** against the existing baseline; the size
report leaves a 255-byte STACK_GAP below the fixed IM2 vector. The reported
SP gap remains 599 bytes because that vector still ends at `0xfd01`.
The 33 statistics bytes occupy the separately checked low-RAM hole, not BSS.
Classic GUI_LOG grows to 1950 bytes: the complete restart prompt, the
`HINTS OFF`/`HINTS ON` texts and `S:STATS ACCEPT RESTART? Y/N`. Next's
extension remains 4522/5376 bytes (854 free); it gains no new
text-rendering stub. The final rendering code exceeds the original 700–1000
byte estimate but stays within the existing ABOUT slot.
SpectraNext closes with `make spectranext-port-build
SPXN_DIR=C:/dev/SpectraNext/driver`; SAVELOAD has **13 bytes free**.
Its larger ABOUT size includes that target's existing About presentation.

Passing software evidence:

- Shared session transcripts and compact/common parity; portable statistics;
  all remaining host checks in the `make test` recipe.
- Review fixes: the compact FSM turns a peer RESET over the local restart
  offer into `S:STATS ACCEPT RESTART? Y/N` (other pending prompts still NACK);
  host key vectors cover chat after OVER, ENTER on the prompt, re-arming,
  a statistics page closed by Y without answering a peer request, and S on
  the opponent's request opening the page while keeping the request.
- 69 byte-for-byte Z80/oracle capture snapshots, including saturation,
  en passant, promotion captures, TAKEBACK, restore, missing SAVELOAD and a
  FASTEST sequence (first move, strict minimum, ties, undo of a new minimum).
  Host oracle tests also cover the share's rounding and 17-bit scaling.
- Nine production-ASM framebuffer cases, including all 30 captures, exact
  approval-PNG pixels, read-only state, outside-page preservation and IY.
  A journal write failure leaves the complete sample and AVG visible.
- 90 native erase cases across Classic, Next and SpectraNext, including
  invalid names, I/O errors and SpectraNext's `.DEL` behavior.
- The GUI_LOG dictionary decodes all 50 messages; restart message 19 must
  equal the public `S:STATS  RESTART? Y/N` macro and message 11 the OVER-only
  `S:STATS ACCEPT RESTART? Y/N`.
- Module guards, both ABI manifests, low-memory checks, size caps, 265 linked
  BOARD vectors per target, linked editor bytes and live-interrupt IY checks.
- `make client-test`: **14/14**. Includes the statistics page, the 30-capture
  layout and modal-dialog keyboard priority.

The final Classic TAP was run in ZEsarUX 13.0 with an isolated SD directory
and a local DIRECT peer: Fool's Mate, resignation after a pawn capture,
accepted draw, S entry, any-key return and explicit Y/ACK RESET all passed.
Pressing Y inside the page only closes it; a second Y requests the restart.
The restored mate board matches the pre-page bitmap and attributes byte for
byte. Empty statistics after RESET contain no captures from the prior game.
After the visual revision the final TAP was re-run in the same harness:
Fool's Mate as Black, the result notice, `S:STATS  RESTART? Y/N`, S, a page
with bare AT numbers and no balance for equal material, any-key return to the
restored board and prompt, then Y → `RESET` → ACK → a new game. The
[actual emulator capture](assets/game-stats-emulator.png) shows that page.

The initial `make -k full-check` exposed a missing statistics dependency in
the SpectraNext recovery test harness. After fixing it, that vector and the
unrun host-recipe tail passed separately. That interim check used
`make full-check -o module-guards -o test -o tap -o nex`.

The closing gate is one unmodified `make full-check`, followed by
`make client-test` (14/14) and `make spectranext-port-build
SPXN_DIR=C:/dev/SpectraNext/driver`. Its first run showed that the NEX was not
relinked after an ABOUT-page edit: `NEXT_NEX_INPUTS` listed overlay sources
explicitly and missed `about/game_stats_render.asm` (INCLUDEd) and
`saveload/game_stats.asm` (linked by `build_overlays.py`), whereas Classic
tracks INCLUDEs itself. `NEXT_NEX_INPUTS` now depends on every
`asm/overlay/*/*.{asm,inc}`. The repeated single `make full-check` relinked the
NEX and passed (ABI, sizes, BOARD, editor and IY on both targets); no cap was
relaxed.

ZEsarUX's installed esxDOS handler rejects journal F_OPEN mode `0x0f`
(read/write/create/truncate). The emulator smoke therefore also exercises
journal failure after successful statistics capture; it cannot certify
journal persistence. Physical Classic, Next and SpectraNext runs remain
pending; the existing Classic stack warning and SpectraNext's 13-byte
SAVELOAD margin require particular attention there.

DIRECT mixed-version checks also passed against the pre-statistics local Qt
binary that declares FileVersion/ProductVersion **1.2.1**, SHA-256
`092e0129fc2315af4bf0a7aab4b087095b9180becb239663bf6d75cdb85def66`:

- New Spectrum resigns: old Qt remains `Restarting...`; Spectrum can show
  statistics, then its explicit RESET receives the old peer's automatic ACK.
- Old Qt resigns: its immediate RESET reaches Spectrum as the opponent's
  restart request (now `S:STATS ACCEPT RESTART? Y/N`); N sends NACK RESET and
  allows statistics. A later explicit RESET is
  accepted through the old Qt confirmation dialog.
- Old Qt offers DRAW: Spectrum accepts, ACK DRAW causes the legacy RESET,
  N refuses that restart, and the DRAW statistics remain accessible.

These checks use the actual local executable and loopback TCP, not a mock of
the old reducer. Its exact originating commit/release manifest is unavailable;
the hash identifies the tested binary. Mixed MQTT and physical peer runs are
not claimed. The existing Qt settings were restored after this check and
verified against their backup; no new client was installed.

Final artifact SHA-256 fingerprints (not a published release):

| Artifact under `release/` | SHA-256 |
| --- | --- |
| `SHATRANJ.tap` | `0727610ffad0c881c3bb90978dba772a917794734f77a29d6d8946ec74c770eb` |
| `SHATRANJ.OVL` | `1055d59522a81cb78cf80e546c6cd86c31ce52bcbd831c6719674b9d27ac6db6` |
| `SHATRANJ.DAT` | `5542a9fb681212fcf1979fc482f7b857a17744a17f339841c4814f1c62f00b8d` |
| `Next/SHATRANJ.nex` | `0807ebb8b5be895833ecba0162f39d484b0a69ddf91765da5b54f101e03e27ef` |
| `Spectranext/SHATRANJ.tap` | `71b88f3d53f3cd6826bd98a5a98e7a74807c9b2d3477c4b38c78e1ffde79371e` |
| `Spectranext/SHATRANJ.OVL` | `6fb5b3a357d1cf20260f8f83ccd9daea52966c4663407c20dc5758e3a945c796` |
| `Spectranext/SHATRANJ.DAT` | `e81a18a318c36712238e96e06cfc02e4f4905fbe3f0a1ccd483d277ad7eb81db` |

The SpectraNext TNFS package `SHATRANJ.PKG` (53,014 bytes) has SHA-256
`510797831f174701531c5d2375e833320369a8becddbc6fe4d08c54d6c7cd4ba`.

These artifacts also include the menu HINTS notice (`HINTS ON` / `HINTS OFF`,
GUI_LOG catalog ids 48/49): Classic resident +11 bytes; SP gap 599
and GUI_LOG +19 bytes (1908/2048), recorded in the size baseline. A ZEsarUX
run toggled HINTS from the menu twice and read both texts from the notice
buffer and screen.

The code-review fixes were re-run on the final Classic TAP in the same
ZEsarUX/DIRECT harness (2026-10-02): after Fool's Mate, ENTER opened chat and
`CHAT gg` reached the peer; a key showed `S:STATS  RESTART? Y/N`, ENTER on it
opened chat (`CHAT ok`) and the next key re-armed it. A peer `RESET` over that
prompt produced `Opponent restart? Y/N` with no reply on the wire; N sent
`NACK RESET`. With the statistics page open, a further peer `RESET` showed the
request in the notice row; Y closed the page without replying and left the
request displayed; a second Y sent `ACK RESET` and started a new game.

The 2026-10-02 page revision was run on the final Classic TAP in the same
harness: four timed moves with one capture per side, the peer's DRAW
accepted, then a 1.2-style `RESET` → `S:STATS ACCEPT RESTART? Y/N`; S opened
the [revised page](assets/game-stats-emulator.png) (TIME 0:35/0:27 with
56%/44%, AVG, MAX/AT, MIN/AT, material 1/1); Y only closed it, the
request remained, and the next Y sent `ACK RESET` and started a new game.
With a 1.2 peer that sends `RESET` after every agreed DRAW, the Spectrum
therefore still offers statistics instead of only `Opponent restart? Y/N`.
The final TAP (aligned discs, MAX/MIN, `S:STATS ACCEPT RESTART? Y/N`) was
re-run the same way; the 27-character prompt fits right of the board and the
page matches the approval layout. A framebuffer case also renders two
ten-character names and checks they stay 4 px clear of the frame.

The host-only restart was run on the final Classic TAP in ZEsarUX, with
the Spectrum as guest against the mock host and as host against a scripted
DIRECT guest. Guest: after OVER a key showed `S:STATS  WAITING OPPONENT` and
Y sent nothing; S opened the statistics and any key returned to the wait;
the host's `RESET` showed `S:STATS ACCEPT RESTART? Y/N`; N sent `NACK RESET`
and `BYE` and returned to Setup; in a second session Y sent `ACK RESET` and
started the game, and BREAK on the next wait sent `BYE` and returned to
Setup. Host: a key showed `S:STATS  RESTART? Y/N`; Y sent `RESET` and showed
`WAITING ACK`, which S did not leave; the guest's `NACK RESET` made the host
send `BYE` and return to Setup; in a second session `ACK RESET` started the
game, and BREAK while the next request waited sent `BYE` and returned to
Setup.

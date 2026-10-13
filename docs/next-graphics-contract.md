# Next presentation separation

## Goal and scope

Allow Next artwork, fonts and composition to evolve without editing the
ZX/SpectraNext presentation. Classic and SpectraNext share ULA rendering;
their placement and loader differences remain explicit. Logical interaction,
rules, session and useful ULA/text primitives remain shared.

Implemented in `codex/next-graphics`, based on `9fda662`, in the independent
`C:/dev/NetChessZX-worktrees/next-graphics` checkout. The separation covers
render roots, resources, layout and presentation effects, preserving the
current appearance. It does not introduce a new artistic design or video mode.

## Decisions and preserved contracts

| Option | Evidence and cost | Decision and verification |
|---|---|---|
| Move Next visual data to `assets/next/` | Values were in `screen.asm`, the hint view, MENU_CONFIG and `gen_assets.py`. Include at the same emission point; no new calls, RAM or bank transitions. | Apply. Compare generated DAT bytes, linked code/overlays, ABI and size. Revert the slice if unexplained emitted changes remain. |
| Reuse `asm_data.py` | The DAT generator already reads labelled DEFB data. | Apply; validate five-byte theme tables. No new format framework or generator. |
| Declare Next-only dependencies | The atlas hashes the whole `asm/` tree; new data under `assets/next/` can be selected explicitly. MENU_CONFIG has its own incremental input list. | Apply. Query Make's actual DAT/resident/atlas prerequisites for each target and test overlay inputs. |
| Split render roots and reuse primitives | Next assembled the Classic root twice, for resident and extension. | Apply: explicit roots, shared ULA includes, original placement/order. Compare linked images and all 49 entry mappings. |
| Select font/layout per target | UI DAT/font and C/ASM coordinates were shared or manually mirrored. | Apply: Next UI source and offsets; selected C headers generate ASM EQU values. Mutation tests prove font/layout independence. |
| Separate panel policy from storage | MENU_CONFIG/TIME_CONFIG wrote literal VRAM addresses; GUI encoded About differences. | Apply: target layout constants and direct-call presentation macros. Preserve buffers, logical row IDs and overlay entry contracts. |
| Remove apparent unused Next DAT bitmaps | Requires proof of every reader; unrelated to ownership of theme data. | Defer. No size savings claimed. |

Keep `render.h`, overlay entries/context, extension's 49 entries, low-RAM,
DATA/BSS ownership, IY/IFF/MMU conventions and all public formats unchanged.
The DAT remains 889 UI bytes plus the existing piece/About payloads. No
pointer or data lifetime changes: includes emit at the original positions.

## Data owners and consumers

| Next-owned input | Consumer / placement |
|---|---|
| `assets/next/ui_theme.asm` | `gen_assets.py --next`: banner and three five-byte tables replace existing DAT slots; not an extra linked blob. |
| `assets/next/theme_constants.asm` | Next renderer and MENU_CONFIG: coordinate attributes/palette indices and wood swatch constants; no emitted storage. |
| `assets/next/coord_palette.asm` | Original coordinate palette position in the Next extension (resident for the diagnostic TAP). |
| `assets/next/hint_inks.asm` | Original Next branch of the hint view table. |
| `assets/next/piece_set_menu.asm` | Original MENU_CONFIG piece-set line. |

Classic/SpectraNext do not consume these inputs. `NEXT_VISUAL_ASSETS` binds
them into Next's atlas and build prerequisites; MENU_CONFIG tracks its two
includes under the production Next compiler flags. Shared files still select
the Next branches, but no longer contain these visual values.

## First data-slice verification (historical)

Before edits, all three DAT variants were generated using the original
generator and retained in `build/next-graphics-baseline/`. After extraction
each is byte-identical: Classic/SpectraNext 7218 bytes, Next 2034 bytes.
This is host generation evidence, not a SpectraNext firmware build.

Linked comparison uses the retained `build/game-actions-checkpoint/` in the
main checkout, not its stale root maps. Its production source is unchanged
between `d74b61c` and `9fda662`. Historical reference: Classic resident/BSS/SP
gap 35078/420/1198; Next 31941/521/4234; Next extension 4808 bytes. The
extension's physical limit is 5376 with 512 reserved: 56 bytes of growth
headroom at that size.

The closing `make full-check PYTHON=C:/Progra~1/Python311/python.exe` passed
(exit 0), recorded in `build/next-graphics-full-check.log`. It includes host
tests, boundary guards, Classic/Next ABI and size gates, and 265 production
BOARD-entry Z80 cases per target. Focused checks also passed: DAT generator
14 tests, Next theme 9 tests, and incremental overlay dependency tests.

| Linked measure | Before | After | Delta |
|---|---:|---:|---:|
| Classic resident / BSS / SP gap | 35078 / 420 / 1198 | 35078 / 420 / 1198 | 0 |
| Next resident / BSS / SP gap | 31941 / 521 / 4234 | 31941 / 521 / 4234 | 0 |
| Next extension | 4808 | 4808 | 0 |
| MENU_CONFIG Classic / Next | 1869 / 1873 | 1869 / 1873 | 0 |

Both CODE binaries and all 17 individual overlays per target are byte-identical
to the checkpoint. The complete Next NEX, Classic TAP and Classic DAT also
match main's retained release bytes. Classic's packed OVL differs only at
offsets 96..99, the four-byte source-binding fingerprint; its payload and
offset table are identical. This source reorganization changes the fingerprint
because shared source and Makefile inputs changed, not because overlay code
changed. No runtime calls, banking operations or storage were added.

First-slice artifact identities (`build/next-graphics-comparison.json`):

| Artifact | SHA-256 |
|---|---|
| Classic TAP | `0f68a184d9d525b4dc36fc761fc2a6029c47620ddc9b45f54d31f10607e152a9` |
| Classic OVL | `e30fefffb52efdeba6a9ef72b737b9684e454a255ef4c510e1731b7fb2246956` |
| Classic DAT | `d89830ddfb6f64cf86edce52e9e2e14310c6e056cc8d088c337ab0441d5c3779` |
| Next NEX | `7bf10b6a4476c98f312a1667866aeaf126843f9b0f26e51272524c87c95eb112` |

The Make database query verified that these new data inputs belong to Next's
DAT/resident/atlas dependencies, not Classic's or SpectraNext's. This does not
remove the older broad `asm/` atlas binding; driver separation remains below.
No interactive emulator or physical-hardware run, dynamic stack measurement,
Qt build or SpectraNext firmware build was performed for this slice.

## Complete ownership and placement

- `asm/spectrum/screen.asm` selects the Classic/SpectraNext board view.
  `asm/next/screen.asm` selects the Next view; the extension includes only
  this Next root. The shared `asm/ula/` fragments remain reusable text/panel/
  address primitives. A different Next panel implementation can replace its
  selected include without changing Classic's root or primitives.
- Next's 96-byte sprite-slot state remains resident; extension accesses remain
  EXTERN imports through the existing map definitions. The 49 stubs/table,
  MMU page 32 and 0x2000 placement are unchanged. Moving source did not move
  objects to another memory class or add calls.
- `layout_zx.h` and `layout_next.h` own visual positions, widths and attributes,
  including FILEUI and direct-VRAM Setup/Time painting. `gen_ui_layout.py`
  emits the selected `ui_layout.inc`; there is no second handwritten ASM map.
  Related ULA page/column constants must be changed together when moving a
  panel. Existing ULA pixel and font metrics remain physical constraints.
- `log_contract.h` owns the existing low-RAM record capacities. Changing
  display coordinates does not resize storage or alter history formatting.
  More stored lines or a new text format require their own measured change.
- `presentation_{zx,next}.h` owns About underlay policy, board hiding and theme
  repaint effects. Macro expansion preserves the existing call sites and
  avoids new frames or bank crossings. `gui.c` remains one orchestrator.
  SpectraNext placement conditions and diagnostic UART-TAP behavior remain.
- `assets/next/ui_runtime_assets.asm` owns Next font/UI data. Its offsets are
  declared in the Next root and validated against the source labels by the
  same DAT generator. The existing DAT format and 889-byte UI payload remain.
  About NXI generation consumes that Next font. Sprites retain the existing
  conversion, bundle offsets, cold loader and staging lifetime.

## Locality scenarios

| Change | Owner after separation |
|---|---|
| Next pieces, board palette, markers, theme/set labels | Next assets and board/hint includes; existing Next converters and bundle loader |
| Next font or panel placement | Next UI asset, `layout_next.h`, and its root-selected panel primitive if drawing behavior changes |
| Next selection/hints | Next board view and marker data; legal masks and interaction unchanged |
| Next resource | Next source/converter, root offsets or extension bundle layout, existing Next packer/loader; memory guards still apply |

Classic atlas binding excludes `asm/next/`, `layout_next.h` and
`presentation_next.h`; Next resource inputs are only selected by Next.
The overlay builder tracks selected layout generation for MENU_CONFIG and
TIME_CONFIG. A common-kernel or Makefile change legitimately affects both.
Sharing the desktop SVG catalog with Next is intentional, separate from ZX.

## Complete-block validation

Focused checks cover layout ownership, font-only DAT mutation, atlas binding
exclusion, all 49 stub/table/offset correspondences, production Setup/Time Z80
vectors, keyboard/coordinates, GUI timers/logs and FILEUI host behavior.
The closing `make full-check PYTHON=C:/Progra~1/Python311/python.exe` passed
(exit 0), retained in `build/next-graphics-complete-full-check.log` and `.exit`.
It includes host/protocol/save/session checks, guards, both ABI manifests,
size gates and 265 linked BOARD-entry Z80 cases per target. No further
firmware gate was run after success.

No new artwork, Layer 2 game screen, removed DAT bitmaps, dynamic dispatch,
new storage capacity or runtime resource registry is required for this plan.
Interactive emulator/hardware acceptance, dynamic stack and application-cycle
measurements are not supplied by source or host checks.

### Linked results for the complete plan

The before-images are the verified first slice, retained before this block in
`build/graphics-phase1/{classic,next}/`. Those production code/overlay bytes
already matched the `9fda662` baseline checkpoint. The final comparison is
`build/next-graphics-complete-comparison.json`.

| Measure | Before | Final | Delta |
|---|---:|---:|---:|
| Classic resident / BSS / SP gap | 35078 / 420 / 1198 | 35078 / 420 / 1198 | 0 |
| Next resident / BSS / SP gap | 31941 / 521 / 4234 | 31941 / 521 / 4234 | 0 |
| Next extension | 4808 | 4808 | 0 |
| MENU_CONFIG Classic / Next | 1869 / 1873 | 1869 / 1873 | 0 |
| TIME_CONFIG Classic / Next | 1912 / 1928 | 1912 / 1928 | 0 |
| FILEUI Classic / Next | 1992 / 2016 | 1992 / 2016 | 0 |

Classic's 19 compared images (CODE, DAT, 17 overlays) and Next's 20
(the same plus extension) are byte-identical. The TAP and NEX also retain
the baseline hashes. The Classic packed OVL has a new source-binding
fingerprint; it must be identified with the final hash below, not the
historical data-slice hash. The older size-check baseline reports savings
from earlier work: this graphical separation adds or saves zero linked bytes.

Final local artifacts, not a claim of new manual or hardware acceptance:

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `release/SHATRANJ.tap` | 35578 | `0f68a184d9d525b4dc36fc761fc2a6029c47620ddc9b45f54d31f10607e152a9` |
| `release/SHATRANJ.OVL` | 28113 | `9ca80bd0b45bae00ae179807f59512bfebcd560ed26cf360157da83d7166dac5` |
| `release/SHATRANJ.DAT` | 7148 | `d89830ddfb6f64cf86edce52e9e2e14310c6e056cc8d088c337ab0441d5c3779` |
| `release/Next/SHATRANJ.nex` | 98816 | `7bf10b6a4476c98f312a1667866aeaf126843f9b0f26e51272524c87c95eb112` |

`build/graphics-dependencies.json` records Make's selected Classic, Next and
SpectraNext DAT/resident dependencies. Tests mutate a Next font byte and a
Next layout value independently, and verify the Classic atlas exclusion.
SpectraNext validation here is dependency selection, host DAT generation and
shared primitive identity; no SpectraNext firmware build or hardware test.

### Manual acceptance and main integration

On 2026-09-12 the user reported that everything worked normally using the
Classic/Next Graphics launchers, then authorized commit to main and push.
This is a positive manual report, not an exhaustive scenario matrix or a
hardware test. It refers to the exact four artifact hashes recorded above.

The temporary owner `netchess-next-graphics` was removed and unregistered.
The canonical descriptor uses owner `netchess`, stable original bundle IDs,
and version `1.2.1-dev.graphics.2`, resolving `--netchess .` from main.
The SpectraNext direct/installer definitions retain their previous artifacts;
this task did not build or update external firmware.

The complete worktree evidence is retained in main under
`build/next-graphics-checkpoint/`: paths written as `build/...` above resolve
under `build/next-graphics-checkpoint/build/...`, and the accepted release
files are also retained under `build/next-graphics-checkpoint/release/`.
Main's `release/` receives the same verified bytes, without another rebuild.
Old main-root maps are not evidence for this change; use the checkpoint maps.
The task branch/worktree can be retired after the fast-forward integration.
Other worktrees and unrelated local changes remain outside this commit.

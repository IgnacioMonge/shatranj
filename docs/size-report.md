# Size report

`make size-report` rebuilds the Spectrum artifacts and writes
`build/size_report.json`.

`make overlay-size OVERLAY=SETUP` (or `GUI_LOG`, `TIME_CONFIG`, …) rebuilds
one Classic overlay against the existing `build/SHATRANJ.map`, checks its
2048-byte cap, and compares it with `docs/size_report.baseline.json`. It does
not relink TAP or pack the atlas. It refreshes `spectrum_config.json` and
fails if the map is missing or older than that stamp; run `make tap` once
in the worktree to create a matching map. Pass
`SIZE_CHECK_FLAGS=--fail-on-growth` when the overlay is expected not to grow.

`make nex-size-report` reports the self-contained Next release. The separate
`make next-size-report` target covers only the diagnostic Next-UART TAP.

`make size-baseline` writes `docs/size_report.baseline.json` after a known-good
build. Commit that baseline before shrink work.

The tracked baseline is a set of independently maintained guardrails, not
necessarily one coherent build snapshot. Do not sum its per-overlay ceilings
or regenerate the whole file unless deliberately establishing a new baseline.

`make size-check` compares the current report with the baseline. By default it
reports deltas without failing on growth. Pass `SIZE_CHECK_FLAGS=--fail-on-growth`
when a branch is expected to be size-neutral or smaller.

`make full-check` is the release gate and always enables `--fail-on-growth`.
The tracked baseline therefore records the deliberately accepted 1.2 image;
later growth must be reviewed and re-baselined explicitly.

Classic has two distinct margins: ordinary resident/BSS growth ends at the
fixed IM2 pointer at `0xfcff`, while `SP_GAP` includes that pointer through
`0xfd00`. Shrinking ordinary code or relocating BSS below the resident image
therefore need not increase the reported stack gap.

The Classic boot capsule reuses `0x6800` until the first CONFIG load. Its bytes
remain in the TAP, so count resident savings separately from tape savings:
the second CODE pair and extra BASIC LOAD add 30 bytes of framing/loader data.
`check_tap_image.py` verifies both CODE payloads, their order, `CLEAR 0x67ff`,
and entry at the resident CRT. Next retains its existing boot packaging.

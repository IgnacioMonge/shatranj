# Overlay ABI guard

`make abi-manifest` writes `build/abi_manifest.json` from the z88dk map,
`src/spectrum/overlay/overlay_api.h`, and `src/spectrum/overlay/overlay.h`.

`make abi-baseline` writes `docs/abi_manifest.baseline.json` after a known-good
`tap` build. Commit that baseline before size-reduction work.

`make abi-check` rebuilds the manifest and compares it with the baseline. It
fails if overlay API declarations, required resident symbol presence, or overlay
ID/entry constants change unexpectedly. Pass `ABI_CHECK_FLAGS=--strict-addresses`
when a branch must also keep resident symbol addresses fixed.

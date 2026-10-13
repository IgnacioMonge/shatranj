# Roller

`roller.inc` is copied byte-for-byte from `C:/dev/Experiments/Roller/roller.inc`.
SHA-256: `8ad9d35580034baa4009a3f7dc50eb48fa3dc6822a90e44195287b9b27ecfb3a`.

The GAME profile (130 bytes) stays vendored for the isolated kernel test.
Production timer/clock UI uses a 4-row nibble blit in `timer_roll.asm`
instead of loading this kernel. Keep vendor edits upstream, then recopy.

# Spectrum connection configuration

`SHATRANJ.CFG` stores Spectrum connection, clock, and game setup.

Classic and Next alternate atomically between `/SYS/SHATRANJ.CFG` and
`/SYS/SHATRANJ.CF2`, and retain `/SYS/CONFIG/SHATRANJ.CFG` as a read-only
legacy fallback. SpectraNext uses `/CFG/SHATRANJ.CFG`. Version 1 is a 46-byte
little-endian record, followed since 1.3 by an 11-byte player-name tail:

| Offset | Size | Value |
| ---: | ---: | --- |
| 0 | 4 | `SHCF` magic |
| 4 | 1 | version (`1`) |
| 5 | 1 | record length (`46`) |
| 6 | 1 | bit 0 JOIN, bit 1 MQTT; remaining bits reserved |
| 7 | 1 | packed COLOR, NOTATION, BOARD, SET, and HINTS choices |
| 8 | 1 | active timezone (`-11..+13`, or `127` for RTC) |
| 9 | 1 | last numeric timezone for RTC fallback |
| 10 | 2 | DIRECT port |
| 12 | 17 | zero-terminated MQTT room (`NC` + four uppercase hexadecimal characters) |
| 29 | 16 | zero-terminated DIRECT host IPv4 text |
| 45 | 1 | CRC-8 polynomial `0x07` over bytes 0..44 |
| 46 | 11 | zero-terminated player name (0-10 of `A-Z 0-9 - _`); 1.3 tail |

The tail sits outside the CRC and the record-length byte stays `46`: a
46-byte 1.2 file loads with no name and a 57-byte file loads the name; any
other size is invalid. A 1.2 build reads a 57-byte file as oversized, reports
`CONFIG INVALID` and starts from defaults. Setup drops a name tail
containing characters outside the allowed set rather than showing or sending
it.

Load validates magic, version, exact size, CRC, reserved bits, timezone,
game-option ranges, port, string bounds, the active MQTT room, and the active
DIRECT JOIN IPv4 before changing live state. Every failure uses compiled
defaults without partially applying the record. A missing file opens normal
Setup, invalid/truncated/oversized data reports `CONFIG INVALID`, and a real
open/read/close failure reports `CONFIG I/O ERROR`.

For JOIN+MQTT, `NC` is fixed and cannot receive the cursor. The editor starts
on the first of the four following characters, accepts `0`-`9` and `A`-`F`
(normalizing lowercase), and requires all four before confirming. Selecting MQTT preserves an existing valid
room; an empty or invalid room becomes `NC` plus an empty suffix and enters the
editor. Selecting any editable endpoint opens it even when its displayed value
is valid. A valid stored DIRECT port (5000 unless changed) counts as answered:
CREATE+DIRECT continues from LINK to TIME, and JOIN+DIRECT continues from an
accepted IP to TIME; PORT stays reachable to change it. Selecting DIRECT with
an empty or invalid port still enters the PORT editor.

The Spectrum Setup screen presents CONNECTION SETUP first: GAME, LINK,
endpoint, TIME and NAME on screen rows 6-10 under a header on row 4, one row
above the board top, so both the blank row before GAME SETUP (rows 12-18) and
the one before SAVE/START (row 20) remain. NAME is logical row 11 (mask bit 11)
but is drawn and navigated between TIME and COLOR. It appears once TIME is
answered, holds GAME SETUP back until confirmed, and accepts an empty value.
Its editor takes up to ten characters, folds lowercase to uppercase, and
ignores anything outside `A-Z 0-9 - _`. `SAVE` writes role,
transport, endpoint, clock source, NAME, COLOR, NOTATION, BOARD, SET, and HINTS;
`EDIT` returns to those fields. Changing any persisted choice marks the record
dirty and refreshes the action row. A successful SAVE keeps the complete menu
visible, changes the action to `EDIT/START`, and focuses START. A failed SAVE
keeps `SAVE/START` with SAVE focused so it can be retried or skipped with START.

Setup rendering is row-masked: normal navigation repaints only the old and new
logical rows, while TIME and ACTION have independent UI flags. Endpoint changes
repaint only GAME/LINK/IP/PORT and the conditional COLOR row. Theme chips keep
their own colours; focus keeps Classic BRIGHT and adds a leading `|`; BOARD
theme chips also keep their four-pixel underline.
JOIN-DIRECT presents IP and PORT as two choices on one row: horizontal arrows
move focus and SPACE/ENTER opens editing, with disjoint attribute spans that do
not recolour the `:` separator. Confirming HINTS reveals SAVE/START immediately.
CONFIG aliases the Setup workspace, so SAVE reinitializes the complete Setup
state after committing or failing as described above.

`TIME` accepts UTC offsets from `-11` through `+13`, or RTC. Next uses the
NextZXOS RTC path, Spectranext uses the cartridge clock path, and Classic probes
the esxDOS driver API, `M_GETDATE`, then the DivTIESUS PCF8563 I2C device. If
RTC is unavailable, every target falls back to the last numeric UTC offset
stored beside the RTC selection. Cold boot prefers RTC whenever the hardware
probe succeeds; the TIME row still exposes UTC so the user can select the saved
numeric fallback explicitly. Emulator success does not replace physical RTC/I2C
validation.
Arrows switch the focused time source without opening the UTC editor; only
SPACE/ENTER starts editing, and other keys remain inert until then.
Classic and Next cold preflight show `CLOCK WAIT` while the RTC probe and SNTP
fallback run, followed by `CLOCK OK` or `CLOCK FAIL`. Spectranext performs its
clock synchronization without adding those UART-panel lines.
Changing between numeric UTC offsets rebases a valid resident clock locally;
it performs no SNTP I/O. Selecting RTC reads the RTC source again; leaving RTC
starts the existing cooperative SNTP retry. The live clock also advances the
FAT date/time used by save-slot names, including timezone and midnight rollover,
without doing network I/O while saving. If cold SNTP failed and the clock is still invalid,
Setup retries cooperatively, one transport poll per idle frame, and cancels the
attempt on START; the retry paints no clock-status message over Setup.

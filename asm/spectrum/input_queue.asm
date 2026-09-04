; Two frame-driven events can arrive between main-loop polls. Priority events
; (BREAK/CANCEL and EDIT/MENU) replace both pending normal events, matching
; the old single-byte latch. Normal events never queue behind a priority one.

KEY_EVENT_CANCEL EQU 0x8a
KEY_EVENT_MENU   EQU 0x90

IFNDEF NETCHESSZX_NEXT_EXTENSION
PUBLIC _spectrum_key_poll
PUBLIC _spectrum_input_poll_event
PUBLIC spectrum_input_queue_clear
PUBLIC spectrum_input_queue_latch
ENDIF

IFDEF NETCHESSZX_NEXT_EXTENSION
EXTERN key_event_0
EXTERN key_event_1
EXTERN spectrum_input_queue_clear
EXTERN spectrum_input_queue_latch
ELSE
SECTION bss_user
key_event_0: DEFS 1
key_event_1: DEFS 1
SECTION code_user
_spectrum_key_poll:
_spectrum_input_poll_event:
    ld a, (key_event_0)
    ld l, a
    ld a, (key_event_1)
    ld (key_event_0), a
    xor a
    ld (key_event_1), a
    ld h, a
    ret

spectrum_input_queue_clear:
    xor a
    ld (key_event_0), a
    ld (key_event_1), a
    ret

; A = event. The scanner produces only nonzero events.
spectrum_input_queue_latch:
    ld b, a
    cp KEY_EVENT_CANCEL
    jr z, spectrum_input_queue_priority
    cp KEY_EVENT_MENU
    jr z, spectrum_input_queue_priority
    ld a, (key_event_0)
    cp KEY_EVENT_CANCEL
    ret z
    cp KEY_EVENT_MENU
    ret z
    or a
    jr z, spectrum_input_queue_store_first
    ld a, (key_event_1)
    or a
    ret nz
    ld a, b
    ld (key_event_1), a
    ret
spectrum_input_queue_store_first:
    ld a, b
    ld (key_event_0), a
    ret

spectrum_input_queue_priority:
    ld a, b
    ld (key_event_0), a
    xor a
    ld (key_event_1), a
    ret
ENDIF

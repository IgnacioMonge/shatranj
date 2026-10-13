IFNDEF NETCHESSZX_TABOPTION_COUNT
NETCHESSZX_TABOPTION_COUNT EQU 6
ENDIF

IFNDEF timer_roll_buffer
; Application glue shared by the timer, clock and menu nibble blits.
timer_roll_state_addr EQU 0x5fe8
IFDEF NETCHESSZX_NEXT_BANKING
timer_roll_buffer EQU 0x3f18
IFNDEF timer_roll_font
timer_roll_font EQU 0x3526
ENDIF
IFNDEF taboption_labels
taboption_labels EQU 0x382c
ENDIF
IFNDEF taboption_transitions
taboption_transitions EQU 0x3863
ENDIF
IFNDEF taboption_actions
taboption_actions EQU 0x386e
ENDIF
ELSE
timer_roll_buffer EQU 0x67cb
IFNDEF timer_roll_font
timer_roll_font EQU 0x6027
ENDIF
IFNDEF taboption_labels
taboption_labels EQU 0x632d
ENDIF
IFNDEF taboption_transitions
taboption_transitions EQU 0x6364
ENDIF
IFNDEF taboption_actions
taboption_actions EQU 0x636f
ENDIF
ENDIF
taboption_cache EQU timer_roll_buffer+5
ENDIF

taboption_visible_addr EQU timer_roll_state_addr+5
taboption_focus_addr EQU timer_roll_state_addr+6
taboption_item_addr EQU timer_roll_state_addr+7

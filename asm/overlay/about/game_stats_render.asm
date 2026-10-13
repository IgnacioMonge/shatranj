; ABOUT entry 1: FILE-sized, read-only end-of-game statistics page.
; Uses only resident pixel/text helpers; never loads another overlay.
; Layout: common/chess/game_stats.h (33 bytes; side entries of 8).
EXTERN _spectrum_game_stats
EXTERN _netchesszx_session_state
EXTERN _spectrum_journal_pend
EXTERN _netchesszx_local_color
EXTERN _netchesszx_board_light_attr
EXTERN _spectrum_render_fileui_frame, _spectrum_render_ikkle_abs_at
EXTERN _spectrum_frame_wait, _spectrum_append_u16
EXTERN compute_screen_base, piece_sprites_16x16

INCLUDE "asm/app/player_name_layout.inc"
IFDEF NETCHESSZX_NEXT_BANKING
gs_asset_base EQU 0x3500
gs_scratch EQU 0x3c78
ELSE
gs_asset_base EQU 0x6001
gs_scratch EQU 0x672b
ENDIF
; Working data lives in the loaded overlay's scratch (lowram_map.h), not in
; the image. Entry zeroes gs_zero and the gs_spec header; every other byte is
; written before it is read. Nothing this page calls touches the scratch.
gs_zero EQU gs_scratch          ; 33 zero bytes: an empty game_stats record
gs_spec EQU gs_zero + 33        ; y, column, attr 0, up to 33 text bytes
gs_flags EQU gs_spec + 36
gs_side EQU gs_flags + 1
gs_row EQU gs_side + 1
gs_kind EQU gs_row + 1
gs_pad EQU gs_kind + 1
gs_type EQU gs_pad + 1
gs_left EQU gs_type + 1
gs_icon_row EQU gs_left + 1
gs_icon_col EQU gs_icon_row + 1
gs_scans EQU gs_icon_col + 1
gs_white_share EQU gs_scans + 1
gs_disc_shift EQU gs_white_share + 1
gs_count EQU gs_disc_shift + 1
gs_entry EQU gs_count + 2
gs_rem EQU gs_entry + 2
gs_out EQU gs_rem + 2
gs_material EQU gs_out + 2
gs_sprite EQU gs_material + 2
gs_source EQU gs_sprite + 2
gs_dest EQU gs_source + 2
gs_scratch_end EQU gs_dest + 2
; Pixel rows of the 4x4 text; value columns are ikkle half-columns.
gs_y_result EQU 66
gs_y_names EQU 82
gs_y_captured EQU 130
gs_y_footer EQU 162
gs_col_white EQU 8
gs_col_black EQU 21

gs_render_page:
    push ix
    push iy
IFDEF NETCHESSZX_SDCC_IY
IFNDEF NETCHESSZX_SPECTRANEXT
    ei
ENDIF
ENDIF
    ld hl, gs_zero
    ld de, gs_zero + 1
    ld bc, 35
    ld (hl), b
    ldir
    ld ix, _spectrum_game_stats
    ld a, (_spectrum_journal_pend)
    bit 7, a
    jr z, gs_sample_live
    and 0x40
    rlca
    rlca
    ld ix, gs_zero
    jr gs_sample_ready
gs_sample_live:
    ld a, (ix+18)
    bit 6, a
    jr z, gs_sample_ready
    ld ix, gs_zero
    ld a, 1
gs_sample_ready:
    ld (gs_flags), a
    call _spectrum_render_fileui_frame
    ld hl, 0x5800+5*32+1
    ld de, 32
    ld c, 16
gs_hide_row:
    push hl
    ld b, 16
gs_hide_cell:
    ld (hl), 0
    inc hl
    djnz gs_hide_cell
    pop hl
    add hl, de
    dec c
    jr nz, gs_hide_row
    ; FILE's header rule would cut through the result bar.
    ld a, 8
    call compute_screen_base
    inc l
    ld b, 16
gs_clear_rule:
    ld (hl), 0
    inc hl
    djnz gs_clear_rule
    call gs_title
    ld hl, (_spectrum_game_stats+19)
    ld a, h
    or l
    jr nz, gs_result_ready
    ld hl, gs_over
gs_result_ready:
    ld b, gs_y_result
    call gs_center
    call gs_share
    ld (gs_white_share), a
    xor a
    ld (gs_side), a
gs_side_loop:
    call gs_names
    call gs_metrics
    call gs_captures
    ld a, (gs_side)
    inc a
    ld (gs_side), a
    cp 2
    jr c, gs_side_loop
    ld hl, gs_labels
    call gs_specs
    ld a, (gs_flags)
    rra
    jr nc, gs_no_partial
    ld hl, gs_partial
    ld bc, gs_y_footer*256+3
    call gs_text
gs_no_partial:
    call gs_reveal
    pop iy
    pop ix
    ld hl, 1
    ret

; HL=string, B=pixel row. Centre within the 32 ikkle columns of FILE.
gs_center:
    push hl
    ld c, 0
gs_length:
    ld a, (hl)
    or a
    jr z, gs_length_done
    inc hl
    inc c
    ld a, c
    cp 32
    jr c, gs_length
gs_length_done:
    ; The approved 17-character result starts at x40 on the 4-pixel grid.
    ld a, c
    cp 17
    jr nz, gs_center_width
    dec c
gs_center_width:
    ld a, 32
    sub c
    srl a
    add a, 2
    ld c, a
    pop hl
; HL=string, B=pixel row, C=ikkle half-column.
gs_text:
    ld a, b
    ld (gs_spec), a
    ld a, c
    ld (gs_spec+1), a
    ld a, 34
    sub c
    ld b, a
    ld de, gs_spec+3
gs_copy_text:
    ld a, (hl)
    or a
    jr z, gs_text_done
    ld (de), a
    inc hl
    inc de
    djnz gs_copy_text
gs_text_done:
    xor a
    ld (de), a
    ld hl, gs_spec
    jp _spectrum_render_ikkle_abs_at

; Packed static rows: y,col,text,NUL; terminated by y zero.
gs_specs:
    ld a, (hl)
    or a
    ret z
    ld b, a
    inc hl
    ld c, (hl)
    inc hl
    push hl
    call gs_text
    pop hl
gs_spec_next:
    ld a, (hl)
    inc hl
    or a
    jr nz, gs_spec_next
    jr gs_specs

; C = this side's value column.
gs_value_col:
    ld a, (gs_side)
    or a
    ld c, gs_col_white
    ret z
    ld c, gs_col_black
    ret

; A = side * 6 (clobbers C).
gs_side6:
    ld a, (gs_side)
    ld c, a
    add a, a
    add a, c
    add a, a
    ret

; The disc starts at the side's value column and the name follows it:
; ten characters fit on both sides, 4 px clear of the FILE frame.
gs_names:
    ld hl, PLAYER_NAME
    ld de, PEER_NAME
    ld a, (_netchesszx_local_color)
    ld c, a
    ld a, (gs_side)
    xor c
    jr z, gs_name_side
    ex de, hl
gs_name_side:
    ld a, (hl)
    or a
    jr nz, gs_name_ready
    ld hl, gs_white
    ld a, (gs_side)
    or a
    jr z, gs_name_ready
    ld hl, gs_black
gs_name_ready:
    call gs_value_col
    inc c
    inc c
    ld b, gs_y_names
    call gs_text
    ; Disc pattern origin x0 = value x - 1 (its ink starts at bit 1):
    ; ULA cell x0 / 8, shifted left by 8 - x0 % 8 across two cells.
    call gs_value_col
    ld a, c
    add a, a
    add a, a
    dec a
    ld c, a
    and 7
    neg
    add a, 8
    ld (gs_disc_shift), a
    ld a, c
    rrca
    rrca
    rrca
    and 31
    push af
    ld a, 10
    call compute_screen_base
    inc h
    pop af
    add a, l
    ld l, a
    ld de, gs_disc
    ld b, 6
gs_disc_row:
    push bc
    push hl
    ld a, (de)
    ld c, a
    ld a, (gs_side)
    or a
    ld a, c
    jr z, gs_disc_shape
    cp 0x42
    jr nz, gs_disc_shape
    ld a, 0x7e
gs_disc_shape:
    ld l, a
    ld h, 0
    ld a, (gs_disc_shift)
gs_disc_shift_loop:
    add hl, hl
    dec a
    jr nz, gs_disc_shift_loop
    ex de, hl
    ex (sp), hl
    ld a, d
    or (hl)
    ld (hl), a
    inc l
    ld a, e
    or (hl)
    ld (hl), a
    dec l
    inc h
    pop de
    inc de
    pop bc
    djnz gs_disc_row
    ret

; Floor/ceil half for black/white, without overflowing ply 65535.
gs_half:
    srl h
    rr l
    ret nc
    ld a, (gs_side)
    or a
    ret nz
    inc hl
    ret

; A = white's rounded share of both totals, 255 without recorded time.
; Scales the 17-bit sum below 640 so white*100 + sum/2 fits 16 bits
; (common/chess/game_stats.c: game_stats_white_share).
gs_share:
    ld l, (ix+0)
    ld h, (ix+1)
    ld e, (ix+8)
    ld d, (ix+9)
    push hl
    add hl, de
    pop de
    sbc a, a
    and 1
    ld c, a
    or h
    or l
    ld a, 255
    ret z
gs_share_scale:
    ld a, c
    or a
    jr nz, gs_share_half
    ld a, h
    cp 2
    jr c, gs_share_div
    jr nz, gs_share_half
    ld a, l
    cp 0x80
    jr c, gs_share_div
gs_share_half:
    srl c
    rr h
    rr l
    srl d
    rr e
    jr gs_share_scale
gs_share_div:
    push hl
    ex de, hl
    add hl, hl
    add hl, hl
    push hl
    add hl, hl
    add hl, hl
    add hl, hl
    push hl
    add hl, hl
    pop bc
    add hl, bc
    pop bc
    add hl, bc
    pop bc
    push bc
    srl b
    rr c
    add hl, bc
    pop bc
    call gs_div
    ld a, l
    ret

gs_metrics:
    ld l, (ix+16)
    ld h, (ix+17)
    call gs_half
    push hl
    ld hl, (_netchesszx_session_state)
    call gs_half
    pop de
    or a
    sbc hl, de
    jr nc, gs_count_ready
    ld hl, 0
gs_count_ready:
    ld a, (_spectrum_game_stats+18)
    bit 6, a
    jr nz, gs_count_empty
    ld a, (_spectrum_journal_pend)
    bit 7, a
    jr z, gs_count_live
gs_count_empty:
    ld hl, 0
gs_count_live:
    ld (gs_count), hl
    push ix
    pop hl
    ld a, (gs_side)
    or a
    jr z, gs_entry_ready
    ld de, 8
    add hl, de
gs_entry_ready:
    ld (gs_entry), hl
    ld hl, gs_rows
gs_metric_loop:
    ld a, (hl)
    or a
    ret z
    ld (gs_row), a
    inc hl
    ld a, (hl)
    inc hl
    push hl
    ld (gs_kind), a
    and 15
    ld e, a
    ld d, 0
    ld hl, (gs_entry)
    add hl, de
    ld e, (hl)
    inc hl
    ld d, (hl)
    ex de, hl
    ld a, (gs_kind)
    or a
    jr z, gs_m_time
    ld de, (gs_count)
    ld a, d
    or e
    jr z, gs_m_empty
    ld a, (gs_kind)
    and 0xf0
    cp 0x20
    jr z, gs_m_duration
    jr nc, gs_m_at
    ; AVG: a saturated total has no meaningful mean.
    call gs_limit_flag
    jr nz, gs_m_empty
    ld b, d
    ld c, e
    call gs_div
    jr gs_m_duration
gs_m_at:
    ld a, l
    inc a
    push af
    ld h, 0
    ld de, gs_spec+3
    call gs_uint
    pop af
    jr nz, gs_m_value
    jr gs_m_plus
gs_m_empty:
    ld hl, gs_empty_text
    jr gs_m_text
gs_m_time:
    call gs_duration
    call gs_limit_flag
    jr z, gs_m_value
gs_m_plus:
    ld a, '+'
    call gs_char
    xor a
    ld (de), a
    jr gs_m_value
gs_m_duration:
    call gs_duration
gs_m_value:
    ld hl, gs_spec+3
gs_m_text:
    call gs_value_col
    ld a, (gs_row)
    ld b, a
    call gs_text
    ld a, (gs_kind)
    or a
    call z, gs_m_share
    pop hl
    jr gs_metric_loop

; NZ when this side's total saturated (TOTAL_LIMIT_WHITE << side).
gs_limit_flag:
    ld a, (gs_side)
    ld c, 2
    or a
    jr z, gs_limit_mask
    sla c
gs_limit_mask:
    ld a, (gs_flags)
    and c
    ret

; Right-aligned share after TIME, omitted without time or after saturation.
gs_m_share:
    ld a, (gs_flags)
    and 6
    ret nz
    ld a, (gs_white_share)
    ld c, a
    inc a
    ret z
    ld a, (gs_side)
    or a
    ld a, c
    jr z, gs_share_side
    ld a, 100
    sub c
gs_share_side:
    ld l, a
    ld h, 0
    ld de, gs_spec+3
    call gs_uint
    ld a, '%'
    call gs_char
    xor a
    ld (de), a
    ld a, (gs_row)
    ld b, a
; DE -> NUL of the text at gs_spec+3, B = pixel row: right-align it to end
; one half-column before the next column / the frame (share, material).
gs_text_right:
    ex de, hl
    ld de, gs_spec+3
    or a
    sbc hl, de
    call gs_value_col
    ld a, c
    add a, 12
    sub l
    ld c, a
    ld hl, gs_spec+3
    jp gs_text

; HL / BC -> HL quotient, DE remainder. BC <= 32768, nonzero.
gs_div:
    ld de, 0
    ld a, 16
gs_div_bit:
    add hl, hl
    rl e
    rl d
    push hl
    ex de, hl
    or a
    sbc hl, bc
    jr nc, gs_div_take
    add hl, bc
    ex de, hl
    pop hl
    jr gs_div_next
gs_div_take:
    ex de, hl
    pop hl
    inc l
gs_div_next:
    dec a
    jr nz, gs_div_bit
    ret

; HL seconds -> NUL string at gs_spec+3, DE points to its NUL.
; TIME pads minutes to two digits; the other durations do not.
gs_duration:
    ld bc, 3600
    call gs_div
    ld (gs_rem), de
    ld de, gs_spec+3
    ld a, h
    or l
    jr z, gs_no_hours
    call gs_uint
    ld a, ':'
    call gs_char
    ld a, 1
    jr gs_duration_pad
gs_no_hours:
    ld a, (gs_kind)
    or a
    ld a, 0
    jr nz, gs_duration_pad
    inc a
gs_duration_pad:
    ld (gs_pad), a
    ld (gs_out), de
    ld hl, (gs_rem)
    ld bc, 60
    call gs_div
    ld (gs_rem), de
    ld de, (gs_out)
    ld a, (gs_pad)
    or a
    jr z, gs_duration_minute
    call gs_uint2
    jr gs_duration_seconds
gs_duration_minute:
    call gs_uint
gs_duration_seconds:
    ld a, ':'
    call gs_char
    ld hl, (gs_rem)
    jp gs_uint2
gs_uint2:
    ld a, l
    cp 10
    jr nc, gs_uint
    ld a, '0'
    call gs_char
gs_uint:
    push hl
    push de
    call _spectrum_append_u16
    ex de, hl
    ret
gs_char:
    ld (de), a
    inc de
    ret

; Miniatures from the active 16x16 set, five per row from ULA cell 4 or
; 10 (under each value column), plus the material those pieces were worth.
gs_captures:
    xor a
    ld (gs_type), a
    ld h, a
    ld l, a
    ld (gs_material), hl
    ld a, 17
    ld (gs_icon_row), a
    call gs_side6
    add a, 4
    ld (gs_icon_col), a
gs_capture_type:
    push ix
    pop hl
    ld de, 32
    add hl, de
    ld a, (gs_type)
    ld e, a
    ld d, 0
    or a
    sbc hl, de
    ld a, (gs_side)
    or a
    ld a, (hl)
    jr z, gs_capture_nibble
    rrca
    rrca
    rrca
    rrca
gs_capture_nibble:
    and 15
    ld (gs_left), a
    jr z, gs_capture_next
    ld a, (gs_type)
    inc a
    ld l, a
    ld h, 0
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    add hl, hl
    ld a, (gs_side)
    or a
    jr z, gs_sprite_mask
    ld de, 32
    add hl, de
gs_sprite_mask:
    ld de, piece_sprites_16x16
    add hl, de
    ld (gs_sprite), hl
    ld a, (gs_side)
    inc a
    ld (gs_threshold+1), a
gs_capture_one:
    ld a, (gs_icon_row)
    cp 20
    jr nc, gs_capture_value
    call compute_screen_base
    ld a, (gs_icon_col)
    add a, l
    ld l, a
    ex de, hl
    ld hl, (gs_sprite)
    call gs_miniature
gs_capture_value:
    ld a, (gs_type)
    ld e, a
    ld d, 0
    ld hl, gs_weights
    add hl, de
    ld e, (hl)
    ld hl, (gs_material)
    add hl, de
    ld (gs_material), hl
    call gs_side6
    add a, 9
    ld hl, gs_icon_col
    inc (hl)
    cp (hl)
    jr nz, gs_capture_step
    sub 5
    ld (hl), a
    ld hl, gs_icon_row
    inc (hl)
gs_capture_step:
    ld a, (gs_left)
    dec a
    ld (gs_left), a
    jr nz, gs_capture_one
gs_capture_next:
    ld a, (gs_type)
    inc a
    ld (gs_type), a
    cp 5
    jp c, gs_capture_type
    ld hl, (gs_material)
    ld de, gs_spec+3
    call gs_uint
    ld b, gs_y_captured
    jp gs_text_right

; HL 16x16 sprite, DE ULA cell. One output bit from each 2x2 block.
; Black/dark threshold 1 retains outlines; white/dark threshold 2 retains fill.
gs_miniature:
    ld (gs_source), hl
    ld (gs_dest), de
    ld a, 8
    ld (gs_scans), a
gs_mini_row:
    ld hl, (gs_source)
    ld b, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld e, (hl)
    inc hl
    ld (gs_source), hl
    ld hl, 0x0800
gs_mini_bit:
    xor a
    sla c
    rl b
    adc a, 0
    sla c
    rl b
    adc a, 0
    sla e
    rl d
    adc a, 0
    sla e
    rl d
    adc a, 0
gs_threshold:
    cp 1
    ccf
    rl l
    dec h
    jr nz, gs_mini_bit
    ld a, l
    ld hl, (gs_dest)
    ld (hl), a
    inc h
    ld (gs_dest), hl
    ld a, (gs_scans)
    dec a
    ld (gs_scans), a
    jr nz, gs_mini_row
    ret

; Exact font_packed -> font_lut -> expand_2x banner at x32,y42.
gs_title:
    ld hl, gs_title_text
    ld de, 0x42a4
gs_title_char:
    ld a, (hl)
    or a
    ret z
    push hl
    push de
    sub 32
    ld l, a
    ld h, 0
    ld c, a
    ld b, 0
    add hl, hl
    add hl, bc
    ld bc, gs_asset_base+166
    add hl, bc
    ld b, 3
gs_title_byte:
    ld c, (hl)
    push bc
    push hl
    ld a, c
    rrca
    rrca
    rrca
    rrca
    call gs_title_nibble
    pop hl
    pop bc
    ld a, c
    push bc
    push hl
    call gs_title_nibble
    pop hl
    pop bc
    inc hl
    djnz gs_title_byte
    pop de
    inc e
    pop hl
    inc hl
    jr gs_title_char
gs_title_nibble:
    and 15
    ld l, a
    ld h, 0
    ld bc, gs_asset_base+16
    add hl, bc
    ld a, (hl)
    rrca
    rrca
    rrca
    rrca
    and 15
    ld l, a
    ld h, 0
    ld bc, gs_asset_base
    add hl, bc
    ld a, (hl)
    ld (de), a
    inc d
    ld (de), a
    inc d
    ld a, d
    and 7
    ret nz
    ld a, d
    sub 8
    ld d, a
    ld a, e
    add a, 32
    ld e, a
    ret

; Only FILE attributes change during the reveal; side panels remain live.
; Row 5 title, rows 8/10 result and names bars, rows 16/20 yellow.
gs_reveal:
    ld a, 5
    ld (gs_row), a
    ld hl, 0x5800+5*32+1
gs_reveal_row:
    ld a, (gs_row)
    ld c, 7
    cp 5
    jr nz, gs_reveal_not_title
    ld c, 0x47
gs_reveal_not_title:
    cp 8
    jr z, gs_reveal_theme
    cp 10
    jr nz, gs_reveal_not_theme
gs_reveal_theme:
    ld a, (_netchesszx_board_light_attr)
    ld c, a
gs_reveal_not_theme:
    ld a, (gs_row)
    cp 16
    jr z, gs_reveal_yellow
    cp 20
    jr nz, gs_reveal_cells
gs_reveal_yellow:
    ld c, 6
gs_reveal_cells:
    push hl
    ld b, 16
gs_reveal_cell:
    ld (hl), c
    inc hl
    djnz gs_reveal_cell
    pop hl
    ld de, 32
    add hl, de
    push hl
    call _spectrum_frame_wait
    pop hl
    ld a, (gs_row)
    inc a
    ld (gs_row), a
    cp 21
    jr c, gs_reveal_row
    ret

; y, kind << 4 | side-entry offset: 0 TIME, 1 AVG, 2 duration, 3 AT.
gs_rows:
    defb 91,0x00, 97,0x10, 103,0x22, 109,0x34, 115,0x25, 121,0x37, 0
gs_labels:
    defb 91,3,"TIME",0,97,3,"AVG",0,103,3,"MAX",0,109,3,"AT",0
    defb 115,3,"MIN",0,121,3,"AT",0,gs_y_captured,3,"CAPTURED",0
    defb gs_y_footer,12,"ANY KEY: BACK",0,0
gs_title_text: defb "GAME STATS",0
gs_over: defb "GAME OVER",0
gs_partial: defb "PARTIAL",0
gs_empty_text: defb "--",0
gs_white: defb "WHITE",0
gs_black: defb "BLACK",0
gs_disc: defb 0x3c,0x42,0x42,0x42,0x42,0x3c
gs_weights: defb 9,5,3,3,1
gs_render_end:

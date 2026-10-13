SECTION code_user

PUBLIC _about_render_ovl_entry
PUBLIC _game_stats_render_ovl_entry

IFDEF NETCHESSZX_NEXT_BANKING

    DEFB 2
    DW _about_render_ovl_entry
    DW _game_stats_render_ovl_entry

; The banking Next build owns a separate Layer 2 About image. Keep only a
; valid atlas entry; the loader never calls this ZX renderer.
_about_render_ovl_entry:
    ld hl, 0
    ret

ELSE

EXTERN ovl_close_overlay_file
EXTERN ovl_read_chunked
EXTERN pixel_down_hl

IFDEF NETCHESSZX_SPECTRANEXT
EXTERN _spxn_dat_bind
EXTERN _esx_fclose
EXTERN _esx_handle
ENDIF

about_row_bytes EQU 32
about_chunk_rows EQU 14
about_chunk_bytes EQU 448
about_tail_rows EQU 12
about_tail_bytes EQU 384
about_pixel_chunks EQU 10
about_attr_tail_rows EQU 5
about_attr_tail_bytes EQU 160
; The band covers screen rows 3-21: scene, one blank row and the key hint.
about_board_top_y EQU 24
; ULA address of that row: top third, scan 0.
about_pixel_base EQU 0x4000 + about_board_top_y * 4
about_board_attr_top EQU 3
about_board_attr_rows EQU 19
about_attr_bytes EQU about_board_attr_rows * about_row_bytes
about_attr_base EQU 0x5800 + (about_board_attr_top * about_row_bytes)
; Transient DAT buffer: reuse the inactive MQTT packet, overlay scratch and
; timer-roll tail while this overlay owns execution. The GUI completes any
; live roll before entering ABOUT.
IFDEF NETCHESSZX_NEXT_BANKING
about_input EQU 0x3B2B
ELSE
IFDEF NETCHESSZX_SPECTRANEXT
about_payload_offset EQU 1273
about_input EQU 0x662B
about_input_size EQU 448
ELSE
; Classic puts the two alternate piece sets before the packed About payload,
; making their seek offsets independent of the compression ratio. Each ZX0
; stream expands in-place at about_input; the directory lives above the
; largest output and the compressor-proven overlap margin.
about_payload_offset EQU 2041
about_directory_bytes EQU 26
about_zx0_delta EQU 3
about_input EQU 0x6616
about_directory EQU about_input + about_chunk_bytes + about_zx0_delta
about_input_size EQU 477
ENDIF
ENDIF

    DEFB 2
    DW _about_render_ovl_entry
    DW _game_stats_render_ovl_entry

_about_render_ovl_entry:
    push ix
    push iy
    ; The dispatcher enters under DI. Classic's resident IM2 handler remains
    ; reachable while DivMMC owns the ROM window; Next retains ROM IM1.
IFDEF NETCHESSZX_SDCC_IY
IFNDEF NETCHESSZX_SPECTRANEXT
    ei
ENDIF

ENDIF
    call ovl_close_overlay_file
IFDEF NETCHESSZX_SPECTRANEXT
    ; Borrow the DAT descriptor the loader retained and seek to the payload;
    ; about_close then only detaches it. A failed bind closes harmlessly.
    ld hl, about_payload_offset
    call _spxn_dat_bind
    ld a, (_esx_handle)
    ld (about_handle), a
    ld a, l
    or a
    jp z, about_fail_close
ELSE
    ld ix, about_asset_filename
    ld b, 0x01
    ld a, '*'
    push iy
    rst 8
    defb 0x9a
    pop iy
    jp c, about_fail
    ld (about_handle), a

    ld de, about_payload_offset
    ld bc, 0
    ld ix, 0
    ld l, 0
    push iy
    rst 8
    defb 0x9f
    pop iy
    jp c, about_fail_close
ENDIF

IFNDEF NETCHESSZX_SPECTRANEXT
    ld ix, about_directory
    ld bc, about_directory_bytes
    ld a, (about_handle)
    call ovl_read_chunked
    jp c, about_fail_close
    ld hl, about_directory
    ld (about_directory_cursor), hl
ENDIF

    ; Zero the band's attributes before copying pixels: the payload paints
    ; pixels top-down while attributes arrive last, so the live board's
    ; colours would otherwise bleed through the reveal. On black the scene
    ; appears hidden, then the two attribute chunks unroll its colours.
    ld hl, about_attr_base
    ld (hl), 0
    ld d, h
    ld e, l
    inc de
    ld bc, about_attr_bytes - 1
    ldir

    ld hl, about_pixel_base
    ld (about_dst), hl
    ld a, about_pixel_chunks
    ld (about_chunks_left), a
about_pixel_chunk_loop:
    ld bc, about_chunk_bytes
    call about_read_input
    ld b, about_chunk_rows
    call about_copy_pixels
    ld a, (about_chunks_left)
    dec a
    ld (about_chunks_left), a
    jr nz, about_pixel_chunk_loop

    ld bc, about_tail_bytes
    call about_read_input
    ld b, about_tail_rows
    call about_copy_pixels

    ld bc, about_chunk_bytes
    call about_read_input
    ld de, about_attr_base
    ld bc, about_chunk_bytes
    ldir
    ld bc, about_attr_tail_bytes
    call about_read_input
    ld de, about_attr_base + about_chunk_bytes
    ld bc, about_attr_tail_bytes
    ldir

about_ok:
    call about_close
    pop iy
    pop ix
    ld hl, 1
    ret

about_fail_close:
    call about_close
about_fail:
    pop iy
    pop ix
    ld hl, 0
    ret

; HL=source, B=row count; about_dst is the next row's ULA address.
about_copy_pixels:
    ld de, (about_dst)
about_copy_pixels_loop:
    push bc
    push de
    ld bc, about_row_bytes
    ldir
    ex (sp), hl
    call pixel_down_hl
    ex de, hl
    pop hl
    pop bc
    djnz about_copy_pixels_loop
    ld (about_dst), de
    ret

; BC=expanded bytes. Returns HL=about_input; any failure leaves the entry
; through about_fail_close instead of returning.
about_read_input:
IFDEF NETCHESSZX_SPECTRANEXT
    ld ix, about_input
    ld a, (about_handle)
    call ovl_read_chunked
ELSE
    ; Place the packed stream so its last byte remains about_zx0_delta
    ; bytes above the last output byte during forward decode.
    push bc
    ld hl, (about_directory_cursor)
    ld c, (hl)
    inc hl
    ld b, (hl)
    inc hl
    ld (about_directory_cursor), hl
    ld a, b
    or c
    pop de
    jr z, about_read_fail
    ld (about_output_left), de
    ld h, d
    ld l, e
    or a
    sbc hl, bc
    jr c, about_read_fail
    ld hl, about_input + about_zx0_delta
    add hl, de
    or a
    sbc hl, bc
    push hl
    push hl
    pop ix
    ld a, (about_handle)
    call ovl_read_chunked
    pop hl
    jr c, about_read_fail
    ld de, about_input
    call about_dzx0_standard
ENDIF
    ld hl, about_input
    ret nc
about_read_fail:
    pop hl                      ; drop the caller's return
    jr about_fail_close

IFNDEF NETCHESSZX_SPECTRANEXT
; Classic ZX0 standard decoder by Einar Saukas, with bounded output.
; HL=source, DE=destination; uses main registers plus A' and preserves IX/IY.
about_dzx0_standard:
    ld bc, 0xffff
    push bc
    inc bc
    ld a, 0x80
about_dzx0_literals:
    call about_dzx0_elias
    call about_dzx0_check_room
    jr c, about_dzx0_overflow
    ldir
    add a, a
    jr c, about_dzx0_new_offset
    call about_dzx0_elias
about_dzx0_copy:
    call about_dzx0_check_room
    jr c, about_dzx0_overflow
    ex (sp), hl
    push hl
    add hl, de
    ldir
    pop hl
    ex (sp), hl
    add a, a
    jr nc, about_dzx0_literals
about_dzx0_new_offset:
    call about_dzx0_elias
    ex af, af'
    pop af
    xor a
    sub c
    ret z
    ld b, a
    ex af, af'
    ld c, (hl)
    inc hl
    rr b
    rr c
    push bc
    ld bc, 1
    call nc, about_dzx0_elias_backtrack
    inc bc
    jr about_dzx0_copy
about_dzx0_overflow:
    pop bc                      ; discard the saved last offset
    scf
    ret
about_dzx0_check_room:
    push af
    push hl
    ld a, b
    or c
    jr z, about_dzx0_no_room
    ld hl, (about_output_left)
    or a
    sbc hl, bc
    jr c, about_dzx0_no_room
    ld (about_output_left), hl
    pop hl
    pop af
    or a
    ret
about_dzx0_no_room:
    pop hl
    pop af
    scf
    ret
about_dzx0_elias:
    inc c
about_dzx0_elias_loop:
    add a, a
    jr nz, about_dzx0_elias_skip
    ld a, (hl)
    inc hl
    rla
about_dzx0_elias_skip:
    ret c
about_dzx0_elias_backtrack:
    add a, a
    rl c
    rl b
    jr about_dzx0_elias_loop
ENDIF

about_close:
    ld a, (about_handle)
IFDEF NETCHESSZX_SPECTRANEXT
    ld (_esx_handle), a
    call _esx_fclose
ELSE
    push iy
    rst 8
    defb 0x9b
    pop iy
ENDIF
    ret

about_handle: DEFB 0
about_dst: DEFW 0
about_chunks_left: DEFB 0
IFNDEF NETCHESSZX_SPECTRANEXT
about_directory_cursor: DEFW 0
about_output_left: DEFW 0
ENDIF
IFNDEF NETCHESSZX_SPECTRANEXT
about_asset_filename:
    DEFM "SHATRANJ.DAT"
    DEFB 0
ENDIF

ENDIF

_game_stats_render_ovl_entry:
    jp gs_render_page

INCLUDE "asm/overlay/about/game_stats_render.asm"

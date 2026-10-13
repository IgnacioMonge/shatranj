draw_square_mark:
    ld a, (tmp_attr)
    ld (mark_mode), a
    jp next_marker_set_mark_current_square

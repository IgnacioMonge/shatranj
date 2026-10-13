; Session-owned bootstrap write operation, assembled inside TIME_CONFIG.
; Inputs: normalized Setup choices. Preserves original instruction order,
; registers, entry ABI and overlay placement; no resident or nested call.
    ld a, (_setup_choice)
    ld (_netchesszx_session_role), a
    ld b, a
    inc a
    and 1
    ld (_netchesszx_host_color_ready), a
    ld a, (_setup_choice + 1)
    ld (_netchesszx_transport), a
    ld a, (_setup_choice + 2)
    ld (_netchesszx_host_color), a
    ld c, a
    ld a, b
    or a
    ld a, c
    jr z, tc_commit_local_color
    xor 1
tc_commit_local_color:
    ld (_netchesszx_local_color), a

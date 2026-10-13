#ifndef NETCHESSZX_SPECTRUM_APP_SESSION_PORTS_H
#define NETCHESSZX_SPECTRUM_APP_SESSION_PORTS_H

/* Compile-time binding for the compact session coordinator.  Keeping these
   semantic effects in the app translation unit preserves the old call/order
   shape without exposing application storage to session policy. */
#define SESSION_APP_CONFIRMATION() ((uint8_t)confirm_action)
#define SESSION_APP_FRAME_COUNT() spectrum_frame_count()
#define SESSION_APP_CONFIRM(kind) (confirm_action = (kind))
#define SESSION_APP_CLEAR_CONFIRMATION() (confirm_action = CONFIRM_NONE)
#define SESSION_APP_HAS_LOCAL_TURN() ((uint8_t)local_turn)
#define SESSION_APP_TAKEBACK_IS_LOCAL() ((uint8_t)takeback_snapshot_local)
#define SESSION_APP_TAKEBACK_PLY() ((uint16_t)takeback_snapshot_ply)
#define SESSION_APP_RESTORE_CHUNK(frame) \
    netchesszx_session_restore_chunk_step(restore_b64_pending, (frame))
/* Classic: the apply ports return 0 when BOARD could not load (nothing
   changed) and BOARD_APPLY runs LOST, which ends the session through
   SESSION_APP_BOARD_LOST or names LOAD FAIL. Next and SpectraNext cannot
   lose BOARD: the apply is not checked and there is no BOARD_LOST. */
#ifdef SPECTRUM_BOARD_RESTORE_CAN_FAIL
#define SESSION_APP_BOARD_APPLY(apply, lost) do { \
    if (!(apply)) { \
        lost; \
    } \
} while (0)
#define SESSION_APP_BOARD_LOST() \
    (disconnect_message = spectrum_app_overlay_load_error)
#else
/* The applies return void here; SDCC rejects a (void) cast of void. */
#define SESSION_APP_BOARD_APPLY(apply, lost) (apply)
#endif
#define SESSION_APP_RESTORE_WORKSPACE() netchesszx_save_meta_t session_app_meta
#define SESSION_APP_RESTORE_DECODE_VALID() \
    (spectrum_restore_decode(restore_b64_pending, &restore_snapshot, &session_app_meta) && \
     restore_host_color_ok(&session_app_meta))
/* The peer's snapshot arrives without history: restart ours as partial
   before apply, so a staged local body is not painted as this position. */
#define SESSION_APP_RESTORE_APPLY() \
    (spectrum_journal_remote_restore(), \
     saveload_apply_snapshot(&session_app_meta, \
                             spectrum_gui_is_board_flipped()))
#define SESSION_APP_RESTORE_CACHE() \
    spectrum_restore_build_b64(&restore_snapshot, &session_app_meta, \
                               restore_b64_pending)
#define SESSION_APP_RESTORE_PREPARE_FRAME(chunk) \
    NETCHESSZX_SAVE_BUILD_RESTORE_FRAME(local_input, restore_b64_pending, chunk)
#define SESSION_APP_RESTORE_FRAME() local_input
/* RESTORE decode already normalizes view_flags to 0/1. */
#define SESSION_APP_APPLY_LOADED(meta) \
    saveload_apply_snapshot((meta), (meta)->view_flags)
#define SESSION_APP_BUILD_LOADED(meta) \
    spectrum_restore_build_b64(&restore_snapshot, (meta), restore_b64_pending)
#define SESSION_APP_BEGIN_SETUP_EXIT() (setup_restart_requested = 1u)
#define SESSION_APP_FINISH_SETUP_EXIT() do { \
    clear_disconnected_session_state(); \
    edit_stop_clear(); \
    spectrum_gui_set_connected(0u); \
} while (0)

#define SESSION_APP_DISCONNECT() handle_opponent_disconnected()
#define SESSION_APP_DISCONNECT_WITH(message) handle_opponent_disconnected_with(message)
#define SESSION_APP_MQTT_PEER_LOST() mqtt_peer_disconnected_wait()
#define SESSION_APP_PEER_LEFT() spectrum_recovery_retire()
#define SESSION_APP_SHOW_READY(message) do { \
    spectrum_gui_set_connected(2u); \
    status_show_endpoint(); \
    ready_notice(message); \
    cursor_show(); \
} while (0)
#define SESSION_APP_SHOW_PEER_MACHINE(platform) do { \
    uint8_t session_app_platform_bits = SPECTRUM_STATUS_PLATFORM_BITS(platform); \
    if ((status_phase_current & SPECTRUM_STATUS_PLATFORM_MASK) != \
        session_app_platform_bits) { \
        status_phase_current = (uint8_t)((status_phase_current & SPECTRUM_STATUS_PHASE_MASK) | \
                                         session_app_platform_bits); \
        status_show_phase(status_phase_current); \
    } \
} while (0)
#define SESSION_APP_SHOW_PEER_NAME(name) do { \
    (void)spectrum_append_text(netchesszx_peer_name, (name)); \
    spectrum_gui_moves_header(); \
} while (0)
#define SESSION_APP_DIRECT_READY(host) do { \
    status_show_endpoint(); \
    if (!(host)) { \
        spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white()); \
        spectrum_gui_moves_header(); \
        notify_wait_msg(SPECTRUM_GUI_MSG_OPPONENT_READY_WAIT); \
    } else { \
        ready_notice(SPECTRUM_GUI_MSG_OPPONENT_READY_GO); \
    } \
} while (0)
#define SESSION_APP_PREPARE_ROOM_CONFLICT() do { \
    if (local_turn) { cursor_hide(); } \
} while (0)
#define SESSION_APP_SHOW_ROOM_CONFLICT() do { \
    spectrum_gui_set_connected(1u); \
    status_show_endpoint(); \
    notify_error_msg(SPECTRUM_GUI_MSG_ROOM_CONFLICT); \
} while (0)
#define SESSION_APP_REFRESH_SIDE() do { \
    spectrum_gui_set_board_view((uint8_t)!netchesszx_local_is_white()); \
    spectrum_gui_redraw_board_view(); \
    spectrum_gui_moves_header(); \
} while (0)
#define SESSION_APP_WAITING_OPPONENT() do { \
    spectrum_gui_set_connected(1u); \
    status_show_endpoint(); \
    notify_wait_msg(SPECTRUM_GUI_MSG_WAITING_OPPONENT); \
} while (0)
#define SESSION_APP_HOST_BUSY_EXIT() do { \
    handle_opponent_disconnected_with(msg_host_busy); \
    wait_after_notice(); \
    setup_restart_requested = 1u; \
} while (0)

#define SESSION_APP_NOTICE_ERROR(text) notify_error(text)
#define SESSION_APP_NOTICE_ERROR_MSG(id) notify_error_msg(id)
#define SESSION_APP_NOTICE_INFO(text) notify_info(text)
#define SESSION_APP_NOTICE_INFO_MSG(id) notify_info_msg(id)
#define SESSION_APP_NOTICE_SUCCESS_MSG(id) notify_success_msg(id)
#define SESSION_APP_NOTICE_WAIT() notify_wait_opponent()
#define SESSION_APP_NOTICE_CONTROL_TIMEOUT(kind) notify_control_timeout(kind)
#define SESSION_APP_REFRESH_STATUS() status_refresh_game()
#define SESSION_APP_SHOW_ENDPOINT() status_show_endpoint()
#define SESSION_APP_SHOW_PERSISTENT(text) spectrum_gui_notify_persistent(text)
#define SESSION_APP_HIDE_MENU() spectrum_gui_hide_menu()
#define SESSION_APP_SHOW_CURSOR() cursor_show()
#define SESSION_APP_HIDE_CURSOR() cursor_hide()
#define SESSION_APP_SUPPRESS_KEY() suppress_current_key()
#define SESSION_APP_SUPPRESS_KEY_UNTIL_RELEASE(key) suppress_key_until_release(key)
#define SESSION_APP_DISCONNECT_SETUP() disconnect_to_setup()
#define SESSION_APP_CLEAR_NOTICE() notify_info("")
#define SESSION_APP_NOTICE_WAIT_MSG(id) notify_wait_msg(id)

#define SESSION_APP_START_GAME() game_start_state()
#define SESSION_APP_RESET_START() reset_auto_start()
#define SESSION_APP_END_GAME(message) end_game_over(message)
#define SESSION_APP_END_DRAW() end_game_over(msg_draw_agreed)
#define SESSION_APP_END_OPPONENT_RESIGN() end_game_over(msg_opponent_resign)
#define SESSION_APP_END_LOCAL_RESIGN() end_game_over(msg_resignation_lost)
#define SESSION_APP_APPLY_TAKEBACK() apply_takeback_snapshot()
#define SESSION_APP_APPLY_PENDING_MOVE(notation) apply_pending_local_move(notation)
#define SESSION_APP_CLEAR_PENDING_MOVE() pending_local_clear()
#define SESSION_APP_REJECT_MOVE() notify_move_rejected()
#define SESSION_APP_WAIT_FOR_ACK() notify_wait_opponent_ack()
#define SESSION_APP_OPPONENT_TURN() notify_error_msg(SPECTRUM_GUI_MSG_OPPONENT_TURN)
#define SESSION_APP_GAME_NOT_STARTED() notify_error_msg(SPECTRUM_GUI_MSG_GAME_NOT_STARTED)
#define SESSION_APP_SEND_BUSY_MOVE_REPLY(ply, control) send_busy_move_reply((ply), (control))
#define SESSION_APP_MOVE_IS_LEGAL(move) spectrum_board_is_legal_move(move)
#define SESSION_APP_SHOW_LOCAL_MOVE(move) do { \
    char session_app_san[SPECTRUM_SAN_TEXT_MAX]; \
    const char *session_app_notice = move_san_or_fallback((move), "", \
                                                          session_app_san); \
    spectrum_gui_notify_persistent(session_app_notice); \
} while (0)
#define SESSION_APP_CLEAR_MOVE_HINTS() movement_hints_clear()
#define SESSION_APP_SHOW_LOCAL_RESIGN() spectrum_gui_notify_persistent(msg_resignation_lost)
#define SESSION_APP_SHOW_WAITING_RESIGN_ACK() \
    spectrum_gui_notify_persistent(msg_waiting_resign_ack)
#define SESSION_APP_NOTICE_LOAD_CANCELLED() notify_info(msg_load_cancelled)
#define SESSION_APP_STOP_INPUT_SHOW_CURSOR() input_stop_and_show_cursor()
#define SESSION_APP_STOP_EDIT_CLEAR() edit_stop_clear()
#define SESSION_APP_ADD_INPUT_HISTORY(text) \
    netchesszx_input_edit_history_add_overlay(text)
/* A remote MOVE is committed, ACKed by the transitions code, then finished:
   the peer's wait for ACK covers the source flash and the board apply, not
   the move log, the destination flash or the journal/checkpoint I/O. */
#define SESSION_APP_REMOTE_MOVE_WORKSPACE() \
    char session_app_san[SPECTRUM_SAN_TEXT_MAX]; \
    const char *session_app_display; \
    uint8_t session_app_san_ready
#define SESSION_APP_APPLY_REMOTE_MOVE(incoming_ply, move, notation, applied) do { \
    session_app_san_ready = move_san_prepare((move), session_app_san); \
    if (session_app_san_ready) { \
        session_app_display = session_app_san; \
    } else { \
        session_app_display = move_display_text((move), (notation)); \
    } \
    spectrum_gui_prepare_move(move); \
    if (spectrum_board_apply_trusted_move_with_undo((move), &spectrum_stats_undo)) { \
        takeback_snapshot_save((incoming_ply), 0u); \
        spectrum_gui_set_board_snapshot(spectrum_board_cells()); \
        netchesszx_session_move_committed(incoming_ply); \
        HOST_SESSION_OBSERVE_REMOTE_MOVE(move); \
        (applied) = 1u; \
    } else { \
        notify_move_rejected(); \
        (applied) = 0u; \
    } \
} while (0)
#define SESSION_APP_FINISH_REMOTE_MOVE(ply_text, move) \
    finish_applied_move((ply_text), (move), session_app_san, \
                        session_app_san_ready, session_app_display, 1u)

#endif

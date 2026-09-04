PUBLIC _spectrum_render_ikkle_at
PUBLIC _spectrum_render_ikkle_abs_at
PUBLIC _spectrum_render_fileui_select
PUBLIC _spectrum_render_fileui_frame

_spectrum_render_board:              call next_extension_restore
                                     jp next_ext_render_board
_spectrum_render_board_area:         call next_extension_restore
                                     jp next_ext_render_board_area
_spectrum_render_board_coords:       call next_extension_restore
                                     jp next_ext_render_board_coords
_spectrum_render_board_coord_mark:   call next_extension_restore
                                     jp next_ext_render_board_coord_mark
_spectrum_render_status:             call next_extension_restore
                                     jp next_ext_render_status
_spectrum_render_status_error:       call next_extension_restore
                                     jp next_ext_render_status_error
_spectrum_render_clock:              call next_extension_restore
                                     jp next_ext_render_clock
_spectrum_render_game_timer_clear:   call next_extension_restore
                                     jp next_ext_render_game_timer_clear
_spectrum_render_game_timer_char:    call next_extension_restore
                                     jp next_ext_render_game_timer_char
_spectrum_render_menu_timer_char:    call next_extension_restore
                                     jp next_ext_render_menu_timer_char
_spectrum_render_turn_label:         call next_extension_restore
                                     jp next_ext_render_turn_label
_spectrum_render_notice:             call next_extension_restore
                                     jp next_ext_render_notice
_spectrum_render_notice_error:       call next_extension_restore
                                     jp next_ext_render_notice_error
_spectrum_render_notice_success:     call next_extension_restore
                                     jp next_ext_render_notice_success
_spectrum_render_connection:         call next_extension_restore
                                     jp next_ext_render_connection
_spectrum_render_menu:               call next_extension_restore
                                     jp next_ext_render_menu
_spectrum_info_show_game:            call next_extension_restore
                                     jp next_ext_info_show_game
_spectrum_info_show_setup:           call next_extension_restore
                                     jp next_ext_info_show_setup
_spectrum_info_show_game_setup:      call next_extension_restore
                                     jp next_ext_info_show_game_setup
_spectrum_render_ikkle_at:           call next_extension_restore
                                     jp next_ext_render_ikkle_at
_spectrum_render_ikkle_abs_at:       call next_extension_restore
                                     jp next_ext_render_ikkle_abs_at
_spectrum_render_fileui_select:      call next_extension_restore
                                     jp next_ext_render_fileui_select
_spectrum_render_fileui_frame:       call next_extension_restore
                                     jp next_ext_render_fileui_frame
_spectrum_info_show_preflight:       call next_extension_restore
                                     jp next_ext_info_show_preflight
_spectrum_info_clear_tail:           call next_extension_restore
                                     jp next_ext_info_clear_tail
_spectrum_info_line:                 call next_extension_restore
                                     jp next_ext_info_line

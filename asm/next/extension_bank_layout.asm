; Fixed slot-1 extension ABI. The page stays mapped at 0x2000 after boot.
next_extension_org                    EQU 0x2000
next_extension_page                   EQU 32
next_extension_code_limit             EQU 0x3500

next_ext_render_board                 EQU next_extension_org
next_ext_render_board_area            EQU next_extension_org + 3
next_ext_render_board_coords          EQU next_extension_org + 6
next_ext_render_board_coord_mark      EQU next_extension_org + 9
next_ext_render_status                EQU next_extension_org + 12
next_ext_render_status_error          EQU next_extension_org + 15
next_ext_render_clock                 EQU next_extension_org + 18
next_ext_render_game_timer_clear      EQU next_extension_org + 21
next_ext_render_game_timer_char       EQU next_extension_org + 24
next_ext_render_menu_timer_char       EQU next_extension_org + 27
next_ext_render_turn_label            EQU next_extension_org + 30
next_ext_render_notice                EQU next_extension_org + 33
next_ext_render_notice_error          EQU next_extension_org + 36
next_ext_render_notice_success        EQU next_extension_org + 39
next_ext_render_connection            EQU next_extension_org + 42
next_ext_render_menu                  EQU next_extension_org + 45
next_ext_info_show_game               EQU next_extension_org + 48
next_ext_info_show_setup              EQU next_extension_org + 51
next_ext_info_show_game_setup         EQU next_extension_org + 54
next_ext_render_ikkle_at              EQU next_extension_org + 57
next_ext_render_ikkle_abs_at          EQU next_extension_org + 60
next_ext_render_fileui_select         EQU next_extension_org + 63
next_ext_render_fileui_frame          EQU next_extension_org + 66
next_ext_info_show_preflight          EQU next_extension_org + 69
next_ext_info_clear_tail              EQU next_extension_org + 72
next_ext_info_line                    EQU next_extension_org + 75
next_ext_set_square_attr_2x2          EQU next_extension_org + 78
next_ext_sprites_hide_all             EQU next_extension_org + 81
next_ext_draw_char64                  EQU next_extension_org + 84
next_ext_key_edit_pressed             EQU next_extension_org + 87
next_ext_input_flush                  EQU next_extension_org + 90
next_ext_input_suppress               EQU next_extension_org + 93
next_ext_input_frame_tick             EQU next_extension_org + 96
next_ext_board_theme_apply            EQU next_extension_org + 99
next_ext_compute_screen_base          EQU next_extension_org + 102
next_ext_compute_attr_base            EQU next_extension_org + 105
next_ext_draw_one_board_square        EQU next_extension_org + 108
next_ext_marker_set_hint              EQU next_extension_org + 111
next_ext_marker_set_mark              EQU next_extension_org + 114
next_ext_draw_square_mark             EQU next_extension_org + 117
next_ext_draw_legal_hints             EQU next_extension_org + 120
next_ext_clear_right_pixel_band       EQU next_extension_org + 123
next_ext_draw_ikkle_text_abs_y        EQU next_extension_org + 126
next_ext_compute_pixel_base           EQU next_extension_org + 129
next_ext_pixel_down_hl                EQU next_extension_org + 132
next_ext_clear_text_row               EQU next_extension_org + 135
next_ext_draw_text64_line_attr        EQU next_extension_org + 138
next_ext_draw_char64_at_tmp           EQU next_extension_org + 141
next_ext_draw_input_cursor            EQU next_extension_org + 144
next_extension_table_size             EQU 147

asset_load_addr                       EQU 0x3500
asset_piece_offset                    EQU 812
piece_sprite_set_size                 EQU 384
next_sprite_stage                     EQU 0x3B2B
next_overlay_scratch                  EQU 0x3C2B
next_palette_stage                    EQU 0x3CCB

next_bundle_dat_offset                EQU 8192
next_sprite_offset                    EQU 16384
next_sprite_set_size                  EQU 3072
next_sprite_pattern_count             EQU 12
next_common_sprite_offset             EQU next_sprite_offset + (3 * next_sprite_set_size)
next_common_sprite_pattern_base       EQU 12
next_board_sprite_pattern_count       EQU 10
next_board_sprite_size                EQU next_board_sprite_pattern_count * 256
next_marker_sprite_offset             EQU next_common_sprite_offset + next_board_sprite_size
next_marker_sprite_pattern_base       EQU 22
next_marker_sprite_pattern_count      EQU 4
sprite_slot_port                      EQU 0x303B
sprite_pattern_port                   EQU 0x005B
nextreg_select                        EQU 0x243B
nextreg_data                          EQU 0x253B
nextreg_sprite_layer_system           EQU 0x15
nextreg_palette_index                 EQU 0x40
nextreg_palette_value_9               EQU 0x44
nextreg_palette_control               EQU 0x43
nextreg_ula_control                   EQU 0x68
nextreg_sprite_transparency_index     EQU 0x4B
next_sprite_transparency              EQU 0xE3
next_about_pal_offset                 EQU 33792
next_about_bank                       EQU 19
layer2_port                           EQU 0x123B
nextreg_layer2_bank                   EQU 0x12
next_sprite_palette_size              EQU 160
next_ula_standard_palette_size        EQU 116
next_sprite_pal_offset                EQU 32768

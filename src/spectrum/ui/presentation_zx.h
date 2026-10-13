#ifndef NETCHESSZX_SPECTRUM_UI_PRESENTATION_ZX_H
#define NETCHESSZX_SPECTRUM_UI_PRESENTATION_ZX_H

#define NETCHESSZX_ABOUT_BLOCKS_UNDERLAY_RENDER 1
#define NETCHESSZX_ABOUT_HIDES_SIDE_PANELS 1
/* The ULA About band offers the changelog page on C. */
#define NETCHESSZX_ABOUT_HAS_CHANGELOG 1
/* ponytail: the changelog page is recognised by its title attribute (bright
   cyan at screen row 4, column 12; asm/overlay/menu_logic/changelog.asm)
   instead of a state byte. Add a flag if another page ever uses that colour
   there. */
#define NETCHESSZX_CHLOG_TITLE_ATTR_ADDR 0x588cu
#define NETCHESSZX_CHLOG_TITLE_ATTR 0x45u

#define spectrum_presentation_close_about() ((void)0)
#define spectrum_presentation_hide_board() ((void)0)
#define spectrum_presentation_restore_about(board) do { \
    spectrum_gui_set_board_snapshot(board); \
    if (spectrum_gui_about_visible() >= 2u) { \
        spectrum_gui_restore_board_area(); \
    } else { \
        spectrum_gui_restore_game_center(); \
    } \
} while (0)
#define spectrum_presentation_apply_theme(theme) netchesszx_board_theme_apply(theme)

#endif

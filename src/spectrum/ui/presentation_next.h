#ifndef NETCHESSZX_SPECTRUM_UI_PRESENTATION_NEXT_H
#define NETCHESSZX_SPECTRUM_UI_PRESENTATION_NEXT_H

#ifdef NETCHESSZX_NEXT_BANKING
#define NETCHESSZX_ABOUT_BLOCKS_UNDERLAY_RENDER 0
#define NETCHESSZX_ABOUT_HIDES_SIDE_PANELS 0
#define NETCHESSZX_ABOUT_HAS_CHANGELOG 0
#define spectrum_presentation_close_about() spectrum_render_about_off()
#define spectrum_presentation_hide_board() spectrum_next_sprites_hide_all()
#define spectrum_presentation_restore_about(board) do { \
    spectrum_render_about_off(); \
    spectrum_gui_set_board_snapshot(board); \
    spectrum_gui_restore_board_area(); \
} while (0)
#else
/* The diagnostic UART TAP still uses the ULA About surface. */
#include "spectrum/ui/presentation_zx.h"
#undef spectrum_presentation_apply_theme
#endif
#define spectrum_presentation_apply_theme(theme) do { \
    netchesszx_board_theme_apply(theme); \
    spectrum_gui_restore_board_area(); \
} while (0)

#endif

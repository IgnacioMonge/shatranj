#include "spectrum/overlay/overlay.h"
#include "spectrum/ui/gui.h"

char spectrum_status_line[SPECTRUM_OVL_STATUS_LINE_SIZE];

void spectrum_gui_status_phase(uint8_t phase) __z88dk_fastcall
{
    spectrum_overlay_context[SPECTRUM_OVL_CTX_STATUS_PHASE] = phase;
    if (spectrum_overlay_exec_cached(SPECTRUM_OVL_STATUS,
                                     SPECTRUM_OVL_STATUS_PHASE)) {
        spectrum_gui_set_status(spectrum_status_line);
    }
}

void spectrum_gui_add_move(const char *ply, const char *move)
{
    uint16_t ply_addr = (uint16_t)ply;
    uint16_t move_addr = (uint16_t)move;

    spectrum_overlay_context[SPECTRUM_OVL_CTX_GUI_MOVE_PLY_LO] =
        (uint8_t)ply_addr;
    spectrum_overlay_context[SPECTRUM_OVL_CTX_GUI_MOVE_PLY_HI] =
        (uint8_t)(ply_addr >> 8);
    spectrum_overlay_context[SPECTRUM_OVL_CTX_GUI_MOVE_TEXT_LO] =
        (uint8_t)move_addr;
    spectrum_overlay_context[SPECTRUM_OVL_CTX_GUI_MOVE_TEXT_HI] =
        (uint8_t)(move_addr >> 8);
    spectrum_overlay_context[SPECTRUM_OVL_CTX_GUI_RENDER] =
        spectrum_gui_side_panels_visible();
    (void)spectrum_overlay_exec_cached(SPECTRUM_OVL_GUI_LOG,
                                       SPECTRUM_OVL_GUI_LOG_ADD_MOVE);
}

void spectrum_gui_add_chat(char who, const char *text)
{
    uint16_t text_addr = (uint16_t)text;

    spectrum_overlay_context[SPECTRUM_OVL_CTX_GUI_CHAT_WHO] = (uint8_t)who;
    spectrum_overlay_context[SPECTRUM_OVL_CTX_GUI_CHAT_TEXT_LO] =
        (uint8_t)text_addr;
    spectrum_overlay_context[SPECTRUM_OVL_CTX_GUI_CHAT_TEXT_HI] =
        (uint8_t)(text_addr >> 8);
    spectrum_overlay_context[SPECTRUM_OVL_CTX_GUI_RENDER] =
        spectrum_gui_side_panels_visible();
    (void)spectrum_overlay_exec_cached(SPECTRUM_OVL_GUI_LOG,
                                       SPECTRUM_OVL_GUI_LOG_ADD_CHAT);
}

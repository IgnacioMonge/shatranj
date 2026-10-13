#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define NETCHESSZX_HOST_TEST

static char input_buffer[64], recall_buffer[128];
#define NETCHESSZX_SPECTRUM_LOWRAM_MAP_H
#define NETCHESSZX_LOWRAM_LOCAL_INPUT_ADDR ((uintptr_t)input_buffer)
#define NETCHESSZX_LOWRAM_LOCAL_INPUT_SIZE sizeof(input_buffer)
#define NETCHESSZX_LOWRAM_INPUT_HISTORY_ADDR ((uintptr_t)recall_buffer)
#define NETCHESSZX_LOWRAM_INPUT_HISTORY_SIZE sizeof(recall_buffer)
#define NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_ADDR 0u
#define NETCHESSZX_LOWRAM_OVERLAY_CONTEXT_SIZE 8u
#include "editor_fixture.h"
#include "spectrum/overlay/input_edit_ovl.c"

uint8_t spectrum_gui_about_visible_state;
void spectrum_render_input_cell(const char *spec) { (void)spec; }
void spectrum_render_input(const char *text) { (void)text; }
void spectrum_gui_set_input(const char *text) { (void)text; }

static void key(uint8_t value)
{
    uint8_t ctx[8] = {0};
    ctx[SPECTRUM_OVL_CTX_INPUT_KEY] = value;
    assert(input_edit_key_ovl(ctx));
}

int main(void)
{
    assert(input_edit_begin_empty_ovl(0) && local_input_mode);
    key('a'); key('c'); key(KEY_LEFT); key('b');
    assert(strcmp(local_input, "abc") == 0 && local_input_cursor == 2);
    key(8);
    assert(strcmp(local_input, "ac") == 0 && local_input_cursor == 1);
    key(KEY_HOME); key(8);
    assert(strcmp(local_input, "ac") == 0 && local_input_cursor == 0);
    key(KEY_END);
    assert(local_input_cursor == 2);
    strcpy(input_history_slot(0), "first");
    strcpy(input_history_slot(1), "second");
    input_history_count = 2;
    key(KEY_UP); assert(strcmp(local_input, "second") == 0);
    key(KEY_UP); key(KEY_UP); assert(strcmp(local_input, "first") == 0);
    key(KEY_DOWN); assert(strcmp(local_input, "second") == 0);
    key(KEY_DOWN); assert(local_input_len == 0);
    key('x');
    assert(input_edit_render_ovl(0) && strcmp(local_input, "x") == 0);
    assert(input_edit_stop_clear_ovl(0) && !local_input_mode && !local_input_len);
    assert(input_history_count == 2);
    puts("UX editing/recall/restore/cancel contracts passed");
    return 0;
}

#include "spectrum/app/time.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "spectrum/lowram_map.h"
static char host_game_timer_line[NETCHESSZX_LOWRAM_GAME_TIMER_SAVE_SIZE];
static uint8_t host_timer_roll_state[NETCHESSZX_LOWRAM_TIMER_ROLL_STATE_SIZE];
static char host_move_lines[NETCHESSZX_LOWRAM_MOVE_LOG_SIZE];
static char host_chat_lines[NETCHESSZX_LOWRAM_CHAT_LOG_SIZE];
#undef NETCHESSZX_LOWRAM_GAME_TIMER_SAVE_ADDR
#define NETCHESSZX_LOWRAM_GAME_TIMER_SAVE_ADDR ((uintptr_t)host_game_timer_line)
#undef NETCHESSZX_LOWRAM_TIMER_ROLL_STATE_ADDR
#define NETCHESSZX_LOWRAM_TIMER_ROLL_STATE_ADDR ((uintptr_t)host_timer_roll_state)
#undef NETCHESSZX_LOWRAM_MOVE_LOG_ADDR
#define NETCHESSZX_LOWRAM_MOVE_LOG_ADDR ((uintptr_t)host_move_lines)
#undef NETCHESSZX_LOWRAM_CHAT_LOG_ADDR
#define NETCHESSZX_LOWRAM_CHAT_LOG_ADDR ((uintptr_t)host_chat_lines)

#include "../../src/spectrum/app/time.c"
#include "../../src/spectrum/ui/gui.c"
#include "../../src/spectrum/app/net_runtime.c"
#include "../../src/spectrum/overlay/mqtt_connect_ovl.c"

static uint8_t clock_renders;
static unsigned game_timer_full_renders;
static char rendered_clock[8];
static unsigned frames_waited;
static unsigned network_drains;
static unsigned uart_pumps;
static uint16_t host_frame_count;
#define TIMER_ROLL_MAX_JOBS 23u
#define TIMER_ROLL_KIND_CLOCK 8u
static uint8_t roll_calls;
static uint8_t roll_q[128];
static uint8_t roll_masks[128];
static uint16_t roll_dests[128];
static uint8_t roll_counts[128];
static uint8_t roll_jobs[128][TIMER_ROLL_MAX_JOBS];
static uint8_t roll_kinds[128];
static uint8_t roll_frame;
static uint8_t roll_frames[128];
static unsigned notice_renders;
static char rendered_notice[NETCHESSZX_NOTICE_TEXT_SIZE];
static char rendered_notice_kind;
static char render_events[16];
static uint8_t render_event_count;
static uint8_t capture_render_events;
static uint8_t about_result = 1u;
static unsigned coord_renders;
static uint8_t menu_timer_phase;
static uint8_t menu_clock_phase;

static void render_event(char event)
{
    if (capture_render_events) {
        assert(render_event_count < sizeof render_events - 1u);
        render_events[render_event_count++] = event;
        render_events[render_event_count] = '\0';
    }
}

static uint16_t host_glyph_dest(uint8_t clock_kind, uint8_t index, uint8_t *mask)
{
    uint8_t col = (uint8_t)((clock_kind != 0u ? 56u : 41u) + index);
    uint16_t base = clock_kind != 0u ? 0x52C0u : 0x4240u;

    *mask = (uint8_t)((col & 1u) != 0u ? 0x0Fu : 0xF0u);
    return (uint16_t)(base + (uint16_t)(col >> 1));
}

/* Host boundary model; target vectors execute the real ASM selection/packing. */
static void host_start_roll(const char *text, char *cache, uint8_t length,
                            uint8_t kind)
{
    uint8_t i, count = 0, mask;
    uint8_t *phase = host_timer_roll_state + (kind != 0u);
    uint8_t q = *phase & 7u;
    assert(roll_calls < sizeof roll_q);
    for (i = 0; i < length; ++i) {
        if (cache[i] != text[i]) {
            if (q == 4u) {
                cache[i] = text[i];
            }
            roll_jobs[roll_calls][count++] = i;
        }
    }
    if (count) {
        *phase |= 128u;
        roll_q[roll_calls] = q;
        roll_frames[roll_calls] = roll_frame;
        roll_dests[roll_calls] = host_glyph_dest(
            kind, roll_jobs[roll_calls][0], &mask);
        roll_masks[roll_calls] = mask;
        roll_counts[roll_calls] = count;
        roll_kinds[roll_calls] = kind;
        ++roll_calls;
    }
}

void netchesszx_asm_timer_game(const char *text)
{ host_start_roll(text, host_game_timer_line, 23, 0); }
void netchesszx_asm_timer_clock(const char *text)
{ host_start_roll(text, host_last_clock_line, 5, TIMER_ROLL_KIND_CLOCK); }
uint8_t netchesszx_asm_menu_key(uint8_t key) { return key; }

uint16_t spectrum_frame_count(void) { return host_frame_count; }
void spectrum_frame_wait(void)
{
    ++host_frame_count;
    ++frames_waited;
}
static void host_app_tick(void)
{
    ++host_frame_count;
    spectrum_app_tick();
}
void spectrum_net_background_drain_clock(void) { ++network_drains; }
void spectrum_uart_background_pump(void) { ++uart_pumps; }
void spectrum_info_line(const char *text) { (void)text; }
void spectrum_render_turn_label(uint8_t mode) { (void)mode; }
void spectrum_render_ikkle_abs_at(const char *spec) { (void)spec; }
void spectrum_render_menu_timer_char(const char *spec) { (void)spec; }
void spectrum_render_menu(uint8_t spec)
{
    assert(spec == 0u);
    assert(menu_visible == 0u);
    menu_timer_phase = timer_roll_phase;
    menu_clock_phase = clock_roll_phase;
    /* Closing surfaces must invalidate coordinates before repainting. */
    spectrum_gui_clear_cursor_coords();
    render_event('M');
}
void spectrum_render_board_coord_mark(const char *spec)
{ (void)spec; ++coord_renders; }
uint8_t spectrum_render_about(void)
{
    assert(timer_roll_phase == 0u && clock_roll_phase == 0u);
    assert(menu_visible == 0u);
    spectrum_gui_clear_cursor_coords();
    render_event('A');
    return about_result;
}
void spectrum_render_connection(uint8_t connected)
{ (void)connected; render_event('D'); }
void spectrum_restore_game_center(const char *board)
{ (void)board; render_event('B'); }
void spectrum_render_board_area(const char *board)
{ (void)board; render_event('B'); }
void spectrum_info_show_game(void) { render_event('I'); }
void spectrum_render_moves(const char *moves)
{ (void)moves; render_event('L'); }
void spectrum_render_chat(const char *chat)
{ (void)chat; render_event('C'); }
void spectrum_gui_moves_header(void) { render_event('H'); }

void spectrum_render_clock(const char *text)
{
    memcpy(rendered_clock, text, sizeof rendered_clock);
    ++clock_renders;
}

static void record_notice(const char *text, char kind)
{
    strcpy(rendered_notice, text);
    rendered_notice_kind = kind;
    ++notice_renders;
    render_event(kind);
}
void spectrum_render_notice(const char *text) { record_notice(text, 'N'); }
void spectrum_render_notice_error(const char *text) { record_notice(text, 'E'); }
void spectrum_render_notice_success(const char *text) { record_notice(text, 'S'); }
void spectrum_render_game_timer_clear(const char *text)
{ (void)text; ++game_timer_full_renders; }
void spectrum_render_game_timer_char(const char *spec) { (void)spec; }

static void test_notice_lifetimes(void)
{
    unsigned tick;
    uint8_t kind;

    about_visible = 0u;
    for (kind = 0u; kind < 2u; ++kind) {
        notice_renders = 0u;
        if (kind) {
            spectrum_gui_notify_success("Saved");
        } else {
            spectrum_gui_notify("Saved", 0u);
        }
        assert(rendered_notice_kind == (kind ? 'S' : 'N'));
        for (tick = 0u; tick < 249u; ++tick) {
            spectrum_gui_present_tick(0u);
        }
        assert(notice_renders == 1u);
        assert(strcmp(rendered_notice, "Saved") == 0);
        spectrum_gui_present_tick(0u);
        assert(notice_renders == 2u);
        assert(rendered_notice[0] == '\0' && rendered_notice_kind == 'N');
        spectrum_gui_present_tick(0u);
        assert(notice_renders == 2u);
    }
    for (kind = 0u; kind < 2u; ++kind) {
        notice_renders = 0u;
        if (kind) {
            spectrum_gui_notify("Retry", 1u);
        } else {
            spectrum_gui_notify_persistent("Retry");
        }
        for (tick = 0u; tick < 300u; ++tick) {
            spectrum_gui_present_tick(0u);
        }
        assert(notice_renders == 1u);
        assert(strcmp(rendered_notice, "Retry") == 0);
        assert(rendered_notice_kind == (kind ? 'E' : 'N'));
    }
}

static void test_fixed_gui_state_initialization(void)
{
    notice_ticks = notice_error = notice_success = 0xa5u;
    last_ply_seen = 0xa5a5u;
    move_line_count = chat_line_count = 0xa5u;
    side_panels_visible = 0u;
    about_visible = 0u;

    spectrum_gui_reset_logs();
    spectrum_gui_notify("", 0u);

    assert(notice_ticks == 250u && notice_error == 0u && notice_success == 0u);
    assert(last_ply_seen == 0u);
    assert(move_line_count == 0u && chat_line_count == 0u);
}

static void start_render_capture(void)
{
    render_event_count = 0u;
    render_events[0] = '\0';
    capture_render_events = 1u;
    coord_renders = 0u;
}

static void test_surface_transitions(void)
{
    uint8_t visible, success;

    memset(&spectrum_time_state, 0, sizeof spectrum_time_state);
    spectrum_time_state.game_timer_active = 1u;
    spectrum_time_state.game_timers[5] = 1u;
    spectrum_time_state.clock_valid = 1u;
    spectrum_time_state.clock_hour = 12u;
    spectrum_time_state.clock_minute = 35u;
    timer_force_redraw = clock_force_redraw = 0u;
    /* ABOUT completes in-flight clock/timer rolls even with no menu open. */
    for (visible = 0u; visible < 2u; ++visible) {
        for (success = 0u; success < 2u; ++success) {
            menu_visible = visible;
            active_coord_valid = 1u;
            about_visible = 0u;
            about_result = success;
            memcpy(host_last_clock_line, "00:00 ", 7u);
            memset(host_game_timer_line, ' ', 23u);
            host_game_timer_line[23] = '\0';
            timer_roll_phase = clock_roll_phase = 1u;
            roll_calls = 0u;
            start_render_capture();
            assert(spectrum_gui_show_about() == success);
            assert(strcmp(render_events, visible ? "MA" : "A") == 0);
            if (visible) {
                assert(menu_timer_phase == 0u && menu_clock_phase == 0u);
            }
            assert(coord_renders == 0u);
            assert(spectrum_gui_about_visible() == success);
            assert(strcmp(host_last_clock_line, "12:35 ") == 0);
            assert(strcmp(host_game_timer_line, "GAME:00h00m TURN:00m01s") == 0);
            assert(spectrum_gui_side_panels_visible() == 0u);
            capture_render_events = 0u;
        }

        menu_visible = visible;
        active_coord_valid = 1u;
        timer_roll_phase = clock_roll_phase = 1u;
        start_render_capture();
        assert(spectrum_gui_show_fileui() == 1u);
        spectrum_gui_clear_cursor_coords();
        assert(strcmp(render_events, visible ? "M" : "") == 0);
        assert(coord_renders == 0u);
        assert(spectrum_gui_fileui_visible() == 1u);
        assert(timer_roll_phase == 1u && clock_roll_phase == 1u);
        capture_render_events = 0u;

        spectrum_gui_show_board_page(3u);
        assert(spectrum_gui_about_visible() == 3u);
        assert(!spectrum_gui_fileui_visible());
        spectrum_gui_restore_board_area();
        assert(!spectrum_gui_about_visible());

        menu_visible = visible;
        start_render_capture();
        spectrum_gui_set_connected(0u);
        assert(strcmp(render_events, visible ? "MD" : "D") == 0);
        capture_render_events = 0u;
    }

    timer_roll_phase = clock_roll_phase = 0u;
    spectrum_gui_notify_persistent("Ready");
    active_coord_valid = 1u;
    start_render_capture();
    spectrum_gui_restore_game_center();
    spectrum_gui_clear_cursor_coords();
    assert(strcmp(render_events, "BILCHN") == 0);
    assert(coord_renders == 0u);
    assert(spectrum_gui_about_visible() == 0u);
    assert(spectrum_gui_side_panels_visible() == 1u);
    assert(strcmp(rendered_notice, "Ready") == 0);
    capture_render_events = 0u;
}

/* Civil time and the stopwatch keep independent fractional seconds. Every
   relative offset must give both consumers five phases on distinct frames. */
static void test_roll_phase_offsets(void)
{
    uint8_t lead, offset, kind, i, phase;

    for (lead = 0u; lead < 2u; ++lead) {
        for (offset = 0u; offset < SPECTRUM_TIME_FRAMES_PER_SECOND; ++offset) {
            memset(&spectrum_time_state, 0, sizeof spectrum_time_state);
            spectrum_time_state.game_timer_active = 1u;
            spectrum_time_state.clock_valid = 1u;
            spectrum_time_state.clock_hour = 12u;
            spectrum_time_state.clock_minute = 34u;
            spectrum_time_state.clock_second = 59u;
            spectrum_time_frames = (uint8_t)(SPECTRUM_TIME_FRAMES_PER_SECOND -
                1u - (lead ? offset : 0u));
            spectrum_time_move_frame_phase = 0u;
            spectrum_time_clock_frames = (uint8_t)(SPECTRUM_TIME_FRAMES_PER_SECOND -
                1u - (lead ? 0u : offset));
            timer_force_redraw = clock_force_redraw = menu_visible = 0u;
            memset(host_timer_roll_state, 0, sizeof host_timer_roll_state);
            memcpy(host_game_timer_line, "GAME:00h00m TURN:00m00s", 24u);
            memcpy(host_last_clock_line, "12:34 ", 7u);
            roll_calls = 0u;
            for (roll_frame = 0u; roll_frame < offset + 5u; ++roll_frame) {
                host_app_tick();
            }
            for (kind = 0u; kind < 2u; ++kind) {
                phase = 0u;
                for (i = 0u; i < roll_calls && phase < 5u; ++i) {
                    if (roll_kinds[i] == (kind ? TIMER_ROLL_KIND_CLOCK : 0u)) {
                        assert(roll_q[i] == phase);
                        assert(roll_frames[i] == phase + (kind == lead ? 0u : offset));
                        ++phase;
                    }
                }
                assert(phase == 5u);
            }
            assert(strcmp(host_last_clock_line, "12:35 ") == 0);
        }
    }
}

int main(void)
{
    const spectrum_time_view_t *time;
    uint8_t i;
    uint8_t saved[6];
    const uint8_t restored[6] = { 1u, 2u, 3u, 4u, 5u, 6u };
    const uint8_t zero[6] = { 0u };
    char formatted[NETCHESSZX_GAME_TIMER_TEXT_SIZE + 1u];

    test_fixed_gui_state_initialization();
    spectrum_app_tick_init();
    spectrum_net_runtime_set_clock(23u, 59u, 59u);
    time = spectrum_time_view();
    if (clock_renders != 1u) {
        fputs("FAIL: setting the clock renders once\n", stderr);
        return 1;
    }
    assert(strcmp(rendered_clock, "[23:59]") == 0);
    for (i = 1u; i < SPECTRUM_TIME_FRAMES_PER_SECOND; ++i) {
        host_app_tick();
    }
    if (time->clock_hour != 23u || time->clock_second != 59u ||
        clock_renders != 1u) {
        fputs("FAIL: application tick advanced before one second\n", stderr);
        return 1;
    }
    host_app_tick();
    if (time->clock_hour != 0u || time->clock_minute != 0u ||
        time->clock_second != 0u || roll_calls == 0u) {
        fputs("FAIL: application tick did not advance and render exactly once\n", stderr);
        return 1;
    }

    /* Real network and overlay wait paths must each deliver one GUI/time tick. */
    spectrum_net_runtime_set_clock(12u, 0u, 0u);
    spectrum_app_game_timer_start();
    for (i = 0u; i < SPECTRUM_TIME_FRAMES_PER_SECOND; ++i) {
        switch (i % 3u) {
        case 0u: spectrum_net_runtime_wait_frame(); break;
        case 1u: spectrum_net_runtime_wait_frame_plain(); break;
        default: preflight_line_ovl("CLOCK"); break;
        }
    }
    assert(frames_waited == SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(network_drains == (SPECTRUM_TIME_FRAMES_PER_SECOND + 2u) / 3u);
    assert(time->clock_second == 1u && time->game_timers[2] == 1u);
    assert(time->game_timers[5] == 1u);

    /* Exercise the production animation wait, not a synthetic tick loop. */
    wait_frames(SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(uart_pumps == 2u * SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(time->clock_second == 2u && time->game_timers[2] == 2u);

    host_app_tick();
    spectrum_app_game_timer_restore(restored);
    build_game_timer_line(formatted);
    assert(strcmp(formatted, "GAME:01h02m TURN:04h05m") == 0);
    spectrum_time_game_timer_save(saved);
    assert(memcmp(saved, restored, sizeof saved) == 0);
    wait_frames((uint8_t)(SPECTRUM_TIME_FRAMES_PER_SECOND - 1u));
    spectrum_time_game_timer_save(saved);
    assert(memcmp(saved, restored, sizeof saved) == 0);
    host_app_tick();
    assert(time->game_timers[2] == 4u && time->game_timers[5] == 7u);

    spectrum_app_move_timer_reset();
    build_game_timer_line(formatted);
    assert(strcmp(formatted, "GAME:01h02m TURN:00m00s") == 0);
    assert(time->game_timers[2] == 4u);
    assert(memcmp(time->game_timers + 3u, zero, 3u) == 0);
    spectrum_app_game_timer_stop();
    build_game_timer_line(formatted);
    assert(strcmp(formatted, "                       ") == 0);
    spectrum_time_game_timer_save(saved);
    wait_frames(SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(memcmp(time->game_timers, saved, sizeof saved) == 0);
    spectrum_app_game_timer_start();
    assert(memcmp(time->game_timers, zero, sizeof zero) == 0);

    spectrum_net_runtime_set_clock(23u, 59u, 59u);
    spectrum_net_runtime_set_fat_stamp(0x4a21u, 0u);
    wait_frames(SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(spectrum_net_runtime_fat_date() == 0x4a21u);
    assert(spectrum_net_runtime_fat_day_delta() == 1);
    assert(spectrum_net_runtime_fat_time() == 0u);
    spectrum_app_shift_clock(-1);
    assert(spectrum_net_runtime_fat_day_delta() == 0);
    assert(time->clock_hour == 23u);
    /* Painting alone must neither advance time nor mutate timers. */
    spectrum_time_game_timer_save(saved);
    for (i = 0u; i < SPECTRUM_TIME_FRAMES_PER_SECOND; ++i) {
        spectrum_gui_present_tick(SPECTRUM_GUI_TIME_CLOCK | SPECTRUM_GUI_TIME_GAME);
    }
    assert(time->clock_hour == 23u && time->clock_second == 0u);
    assert(memcmp(saved, time->game_timers, sizeof saved) == 0);
    saved[3] = 0u;
    saved[4] = 12u;
    saved[5] = 34u;
    spectrum_app_game_timer_restore(saved);
    build_game_timer_line(formatted);
    assert(memcmp(formatted + 17u, "12m34s", 6u) == 0);

    /* Ordinary second: only TURN seconds units (index 21, $F0 at 0x425F). */
    spectrum_app_game_timer_stop();
    spectrum_app_game_timer_start();
    saved[0] = 0u;
    saved[1] = 0u;
    saved[2] = 0u;
    saved[3] = 0u;
    saved[4] = 0u;
    saved[5] = 0u;
    spectrum_app_game_timer_restore(saved);
    spectrum_net_runtime_set_clock(12u, 34u, 0u);
    memcpy(host_last_clock_line, "12:34 ", 7u);
    host_timer_roll_state[0] = 0u;
    for (i = 0u; i < 4u; ++i) {
        spectrum_gui_present_tick(0u);
    }
    roll_calls = 0u;
    wait_frames(SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(roll_calls == 1u);
    assert(roll_q[0] == 0u);
    assert(roll_counts[0] == 1u);
    assert(roll_jobs[0][0] == 21u);
    assert(roll_kinds[0] == 0u);
    assert(roll_masks[0] == 0xF0u);
    assert(roll_dests[0] == 0x425Fu);
    for (i = 0u; i < 4u; ++i) {
        spectrum_gui_present_tick(0u);
    }
    assert(roll_calls == 5u);
    for (i = 0u; i < 5u; ++i) {
        assert(roll_q[i] == i);
        assert(roll_dests[i] == 0x425Fu);
        assert(roll_masks[i] == 0xF0u);
        assert(roll_jobs[i][0] == 21u);
    }

    /* 09 → 10: seconds tens ($0F at 0x425E) and units ($F0 at 0x425F). */
    saved[5] = 9u;
    spectrum_app_game_timer_restore(saved);
    memcpy(host_game_timer_line, "GAME:00h00m TURN:00m09s", 24u);
    host_timer_roll_state[0] = 0u;
    roll_calls = 0u;
    wait_frames(SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(roll_calls == 1u);
    assert(roll_counts[0] == 2u);
    assert(roll_jobs[0][0] == 20u);
    assert(roll_jobs[0][1] == 21u);
    {
        uint8_t mask_tens;
        uint8_t mask_units;
        assert(host_glyph_dest(0u, 20u, &mask_tens) == 0x425Eu);
        assert(mask_tens == 0x0Fu);
        assert(host_glyph_dest(0u, 21u, &mask_units) == 0x425Fu);
        assert(mask_units == 0xF0u);
    }
    for (i = 0u; i < 4u; ++i) {
        spectrum_gui_present_tick(0u);
    }
    assert(roll_calls == 5u);
    for (i = 0u; i < 5u; ++i) {
        assert(roll_q[i] == i);
        assert(roll_counts[i] == 2u);
    }

    /* Clock without a minute change: zero Roller jobs. */
    spectrum_app_game_timer_stop();
    spectrum_net_runtime_set_clock(12u, 34u, 30u);
    memcpy(host_last_clock_line, "12:34 ", 7u);
    host_timer_roll_state[0] = 0u;
    roll_calls = 0u;
    wait_frames(SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(time->clock_minute == 34u);
    assert(roll_calls == 0u);

    /* 12:34 → 12:35: only the last clock digit ($F0 at 0x52DE). */
    spectrum_net_runtime_set_clock(12u, 34u, 59u);
    memcpy(host_last_clock_line, "12:34 ", 7u);
    roll_calls = 0u;
    wait_frames(SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(roll_calls == 1u);
    assert(roll_kinds[0] == TIMER_ROLL_KIND_CLOCK);
    assert(roll_counts[0] == 1u);
    assert(roll_jobs[0][0] == 4u);
    assert(roll_masks[0] == 0xF0u);
    assert(roll_dests[0] == 0x52DEu);
    for (i = 0u; i < 4u; ++i) {
        spectrum_gui_present_tick(0u);
    }
    assert(roll_calls == 5u);

    /* 12:59 → 13:00: exactly the three modified digits. */
    spectrum_net_runtime_set_clock(12u, 59u, 59u);
    memcpy(host_last_clock_line, "12:59 ", 7u);
    host_timer_roll_state[0] = 0u;
    roll_calls = 0u;
    wait_frames(SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(roll_calls == 1u);
    assert(roll_counts[0] == 3u);
    assert(roll_jobs[0][0] == 1u);
    assert(roll_jobs[0][1] == 3u);
    assert(roll_jobs[0][2] == 4u);
    {
        uint8_t mask_h;
        uint8_t mask_mt;
        uint8_t mask_mu;
        assert(host_glyph_dest(TIMER_ROLL_KIND_CLOCK, 1u, &mask_h) == 0x52DCu);
        assert(mask_h == 0x0Fu);
        assert(host_glyph_dest(TIMER_ROLL_KIND_CLOCK, 3u, &mask_mt) == 0x52DDu);
        assert(mask_mt == 0x0Fu);
        assert(host_glyph_dest(TIMER_ROLL_KIND_CLOCK, 4u, &mask_mu) == 0x52DEu);
        assert(mask_mu == 0xF0u);
    }

    /* Timer and clock change together; force-redraw finishes both. */
    spectrum_app_game_timer_start();
    saved[0] = 0u;
    saved[1] = 0u;
    saved[2] = 0u;
    saved[3] = 0u;
    saved[4] = 0u;
    saved[5] = 0u;
    spectrum_app_game_timer_restore(saved);
    memcpy(host_game_timer_line, "GAME:00h00m TURN:00m00s", 24u);
    memcpy(host_last_clock_line, "12:34 ", 7u);
    spectrum_net_runtime_set_clock(12u, 34u, 59u);
    host_timer_roll_state[0] = 0u;
    roll_calls = 0u;
    wait_frames(SPECTRUM_TIME_FRAMES_PER_SECOND);
    assert(roll_calls >= 1u);
    assert(roll_kinds[0] == 0u);
    assert(roll_calls == 2u);
    assert(roll_kinds[1] == TIMER_ROLL_KIND_CLOCK);
    assert(roll_q[0] == 0u && roll_q[1] == 0u);
    assert(host_timer_roll_state[0] != 0u);
    game_timer_full_renders = 0u;
    spectrum_gui_time_changed(SPECTRUM_GUI_TIME_FORCE | SPECTRUM_GUI_TIME_GAME |
                              SPECTRUM_GUI_TIME_CLOCK);
    assert(host_timer_roll_state[0] == 0u && host_timer_roll_state[1] == 0u);
    /* A full timer paint already completes it; only the clock needs Roller. */
    assert(game_timer_full_renders == 1u);
    assert(roll_calls == 3u && roll_kinds[2] == TIMER_ROLL_KIND_CLOCK);

    /* No queue/batch ceiling: all ten changed cells share the five phases.
       Cache remains old until the last phase, so unchanged cells never move. */
    spectrum_app_game_timer_start();
    memcpy(host_game_timer_line, "GAME:99h59m TURN:99h59m", 24u);
    roll_calls = 0u;
    render_game_timer_only();
    assert(roll_counts[0] == 10u);
    assert(strcmp(host_game_timer_line, "GAME:99h59m TURN:99h59m") == 0);
    for (i = 0u; i < 4u; ++i) {
        spectrum_gui_present_tick(0u);
    }
    assert(roll_calls == 5u);
    for (i = 0u; i < 5u; ++i) {
        assert(roll_q[i] == i && roll_counts[i] == 10u);
    }
    assert(host_timer_roll_state[0] == 0u);
    assert(strcmp(host_game_timer_line, "GAME:00h00m TURN:00m00s") == 0);

    test_roll_phase_offsets();
    test_notice_lifetimes();
    test_surface_transitions();
    puts("gui/network/overlay timer tests and all phase offsets passed");
    return 0;
}

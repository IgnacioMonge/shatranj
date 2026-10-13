#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "spectrum/config/session.h"
#include "spectrum/session/state.h"

static void expect_session(uint8_t role,
                           uint8_t transport,
                           uint8_t host_color,
                           uint8_t local_color)
{
    assert(netchesszx_session_role == role);
    assert(netchesszx_transport == transport);
    assert(netchesszx_host_color == host_color);
    assert(netchesszx_local_color == local_color);
}

int main(void)
{
    netchesszx_session_schedule_t schedule;

    expect_session(NETCHESSZX_SESSION_ROLE_HOST,
                   NETCHESSZX_TRANSPORT_MQTT,
                   NETCHESSZX_COLOR_WHITE,
                   NETCHESSZX_COLOR_WHITE);

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_HOST,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_BLACK);
    expect_session(NETCHESSZX_SESSION_ROLE_HOST,
                   NETCHESSZX_TRANSPORT_MQTT,
                   NETCHESSZX_COLOR_BLACK,
                   NETCHESSZX_COLOR_BLACK);

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_BLACK);
    expect_session(NETCHESSZX_SESSION_ROLE_JOIN,
                   NETCHESSZX_TRANSPORT_MQTT,
                   NETCHESSZX_COLOR_BLACK,
                   NETCHESSZX_COLOR_WHITE);

    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                                 NETCHESSZX_TRANSPORT_MQTT,
                                 NETCHESSZX_COLOR_WHITE);
    expect_session(NETCHESSZX_SESSION_ROLE_JOIN,
                   NETCHESSZX_TRANSPORT_MQTT,
                   NETCHESSZX_COLOR_WHITE,
                   NETCHESSZX_COLOR_BLACK);

    netchesszx_session_configure(99u, 99u, 99u);
    expect_session(NETCHESSZX_SESSION_ROLE_HOST,
                   NETCHESSZX_TRANSPORT_MQTT,
                   NETCHESSZX_COLOR_WHITE,
                   NETCHESSZX_COLOR_WHITE);

    /* Probe identity must not confirm seating or overwrite the last host
       colour: the live H/HELLO, not a retained snapshot, owns that decision. */
    netchesszx_session_forget_color();
    netchesszx_session_probe_identity(NETCHESSZX_COLOR_BLACK, 73u);
    assert(!netchesszx_session_identity_host_color_ready());
    assert(netchesszx_session_identity_local_color() == NETCHESSZX_COLOR_WHITE);
    assert(netchesszx_session_identity_host_color() == NETCHESSZX_COLOR_WHITE);
    assert(netchesszx_session_identity_mqtt_session_id() == 73u);
    netchesszx_session_configure(NETCHESSZX_SESSION_ROLE_JOIN,
                               NETCHESSZX_TRANSPORT_MQTT,
                               NETCHESSZX_COLOR_BLACK);
    assert(netchesszx_session_identity_host_color_ready());
    assert(netchesszx_session_identity_mqtt_session_id() == 73u);

    assert(netchesszx_session_view()->game_ply == 0u);
    netchesszx_session_set_game_ply(42u);
    netchesszx_session_set_start_pending(1u);
    netchesszx_session_set_restore_rx_mask(0x10u);
    assert(netchesszx_session_view()->game_ply == 42u);
    assert(netchesszx_session_view()->start_pending == 1u);
    assert(netchesszx_session_view()->restore_rx_mask == 0x10u);
    netchesszx_session_set_restore_rx_mask(0u);
    assert(netchesszx_session_view()->game_ply == 42u);
    assert(netchesszx_session_view()->start_pending == 1u);

    netchesszx_session_schedule_set_pending_retry_wait(&schedule, 2u);
    netchesszx_session_schedule_tick_pending_retry_wait(&schedule);
    assert(netchesszx_session_schedule_view(&schedule)->pending_retry_wait == 1u);
    netchesszx_session_schedule_set_control_cancel_wait(&schedule, 300u);
    assert(netchesszx_session_schedule_view(&schedule)->control_cancel_wait == 300u);

    puts("session config/identity/state tests ok");
    return 0;
}

#include "common/session/session.h"
#include "common/session/session_internal.h"

#include "common/protocol/platform_protocol.h"

#include <string.h>

static uint8_t session_config_valid(const SessionConfig *config)
{
    return (uint8_t)(config != 0 &&
                     config->transport <= SESSION_TRANSPORT_MQTT &&
                     config->role <= SESSION_ROLE_GUEST &&
                     (config->host_color <= SESSION_COLOR_BLACK ||
                      config->host_color == SESSION_COLOR_UNKNOWN));
}

uint8_t session_text_length(const char *text)
{
    uint8_t length = 0u;

    while (length < SESSION_PAYLOAD_MAX && text[length] != '\0') {
        ++length;
    }
    return length;
}

uint8_t session_text_equal(const uint8_t *payload,
                           uint8_t length,
                           const char *text)
{
    uint8_t i;

    for (i = 0u; i < length; ++i) {
        if (text[i] == '\0' || payload[i] != (uint8_t)text[i]) {
            return 0u;
        }
    }
    return (uint8_t)(text[length] == '\0');
}

uint8_t session_text_prefix(const uint8_t *payload,
                            uint8_t length,
                            const char *prefix)
{
    uint8_t i = 0u;

    while (prefix[i] != '\0') {
        if (i >= length || payload[i] != (uint8_t)prefix[i]) {
            return 0u;
        }
        ++i;
    }
    return 1u;
}

uint8_t session_slice_valid(const uint8_t *payload, uint8_t length)
{
    uint8_t i;

    if (payload == 0 || length > SESSION_PAYLOAD_MAX || payload[length] != 0u) {
        return 0u;
    }
    for (i = 0u; i < length; ++i) {
        if (payload[i] == 0u) {
            return 0u;
        }
    }
    return 1u;
}

uint8_t session_fixed_slice_valid(const uint8_t *payload, uint8_t length)
{
    uint8_t i;

    if (payload == 0) {
        return 0u;
    }
    for (i = 0u; i < length; ++i) {
        if (payload[i] == 0u) {
            return 0u;
        }
    }
    return 1u;
}

uint8_t session_parse_mach(const uint8_t *payload,
                           uint8_t length,
                           uint8_t *platform)
{
    uint8_t value;

    if (platform == 0 || payload == 0 ||
        !session_text_prefix(payload, length, NETCHESS_PROTO_MACH_PREFIX)) {
        return 0u;
    }
    payload += sizeof(NETCHESS_PROTO_MACH_PREFIX) - 1u;
    length = (uint8_t)(length - (sizeof(NETCHESS_PROTO_MACH_PREFIX) - 1u));
    if (length == 2u && payload[0] == 'Z' && payload[1] == 'X') {
        value = NETCHESS_PLAT_ZX;
    } else if (length == 3u && payload[0] == 'N' && payload[1] == 'X' &&
               payload[2] == 'T') {
        value = NETCHESS_PLAT_NXT;
    } else if (length == 3u && payload[0] == 'M' && payload[1] == 'A' &&
               payload[2] == 'C') {
        value = NETCHESS_PLAT_MAC;
    } else if (length == 3u && payload[0] == 'L' && payload[1] == 'N' &&
               payload[2] == 'X') {
        value = NETCHESS_PLAT_LNX;
    } else if (length == 2u && payload[0] == 'P' && payload[1] == 'C') {
        value = NETCHESS_PLAT_PC;
    } else if (length == 4u && payload[0] == 'S' && payload[1] == 'P' &&
               payload[2] == 'C' && payload[3] == 'X') {
        value = NETCHESS_PLAT_SPCX;
    } else {
        return 0u;
    }
    *platform = value;
    return 1u;
}

char *session_u16_text(char *out, uint16_t value)
{
    static const uint16_t places[5] = {10000u, 1000u, 100u, 10u, 1u};
    uint16_t place;
    uint8_t digit;
    uint8_t i;
    uint8_t started = 0u;

    for (i = 0u; i < 5u; ++i) {
        place = places[i];
        digit = 0u;
        while (value >= place) {
            value = (uint16_t)(value - place);
            ++digit;
        }
        if (digit != 0u || started != 0u || place == 1u) {
            *out++ = (char)('0' + digit);
            started = 1u;
        }
    }
    *out = '\0';
    return out;
}

uint16_t session_parse_u16(const char *text)
{
    uint16_t value = 0u;
    uint8_t digit;

    if (*text < '0' || *text > '9') {
        return 0u;
    }
    while (*text >= '0' && *text <= '9') {
        digit = (uint8_t)(*text - '0');
        if (value > 6553u || (value == 6553u && digit > 5u)) {
            return 0u;
        }
        value = (uint16_t)(value * 10u + digit);
        ++text;
    }
    return *text == '\0' ? value : 0u;
}

uint8_t session_emit_timer_cancel(SessionState *state,
                                  SessionAction *actions,
                                  uint8_t *count,
                                  uint8_t timer_id)
{
    uint8_t bit = (uint8_t)(1u << timer_id);

    if ((state->timer_mask & bit) == 0u) {
        return 1u;
    }
    if (*count >= SESSION_ACTION_CAPACITY) {
        return 0u;
    }
    actions[*count].type = SESSION_ACT_TIMER_CANCEL;
    actions[*count].data.timer_cancel.timer_id = timer_id;
    ++*count;
    state->timer_mask &= (uint8_t)~bit;
    return 1u;
}

uint8_t session_emit_session(SessionAction *actions,
                             uint8_t *count,
                             uint8_t status)
{
    if (*count >= SESSION_ACTION_CAPACITY) {
        return 0u;
    }
    actions[*count].type = SESSION_ACT_SESSION_CHANGED;
    actions[*count].data.session.status = status;
    actions[*count].data.session.end_reason = SESSION_END_REASON_NONE;
    ++*count;
    return 1u;
}

uint8_t session_emit_end(SessionAction *actions,
                         uint8_t *count,
                         uint8_t end_reason)
{
    if (!session_emit_session(actions, count, SESSION_CHANGED_ENDED)) {
        return 0u;
    }
    actions[(uint8_t)(*count - 1u)].data.session.end_reason = end_reason;
    return 1u;
}

uint8_t session_emit_side(SessionState *state,
                          SessionAction *actions,
                          uint8_t *count)
{
    if (*count >= SESSION_ACTION_CAPACITY) {
        return 0u;
    }
    actions[*count].type = SESSION_ACT_SIDE_CHANGED;
    actions[*count].data.side.color = state->local_color;
    actions[*count].data.side.session_id = state->session_id;
    ++*count;
    return 1u;
}

uint8_t session_emit_close(SessionAction *actions,
                           uint8_t *count,
                           uint8_t link_id)
{
    if (*count >= SESSION_ACTION_CAPACITY) {
        return 0u;
    }
    actions[*count].type = SESSION_ACT_LINK_CLOSE;
    actions[*count].data.link_close.link_id = link_id;
    ++*count;
    return 1u;
}

uint8_t session_emit_game(SessionAction *actions,
                          uint8_t *count,
                          uint8_t kind,
                          uint8_t delivery_id,
                          uint16_t value,
                          const uint8_t *payload,
                          uint8_t length)
{
    if (*count >= SESSION_ACTION_CAPACITY) {
        return 0u;
    }
    actions[*count].type = SESSION_ACT_DELIVER_GAME;
    actions[*count].data.game.kind = kind;
    actions[*count].data.game.delivery_id = delivery_id;
    actions[*count].data.game.value = value;
    actions[*count].data.game.payload = payload;
    actions[*count].data.game.length = length;
    ++*count;
    return 1u;
}

uint8_t session_emit_decision(SessionAction *actions,
                              uint8_t *count,
                              uint8_t request_id,
                              uint8_t control,
                              uint16_t value)
{
    if (*count >= SESSION_ACTION_CAPACITY) {
        return 0u;
    }
    actions[*count].type = SESSION_ACT_REQUEST_DECISION;
    actions[*count].data.decision.request_id = request_id;
    actions[*count].data.decision.control = control;
    actions[*count].data.decision.value = value;
    ++*count;
    return 1u;
}

uint8_t session_next_tx_id(SessionState *state)
{
    uint8_t id = state->next_tx_id;

    ++state->next_tx_id;
    if (state->next_tx_id == 0u) {
        state->next_tx_id = 1u;
    }
    if (id == 0u) {
        id = state->next_tx_id++;
    }
    return id;
}

uint8_t session_next_delivery_id(SessionState *state)
{
    ++state->delivery_id;
    if (state->delivery_id == 0u) {
        ++state->delivery_id;
    }
    return state->delivery_id;
}

void session_clear_duplicate(SessionState *state)
{
    state->last_rx_kind = 0u;
    state->last_value = 0u;
    state->last_result = 0u;
}

void session_drop_restore_cache(SessionState *state)
{
    if (state->restore_phase == SESSION_RESTORE_PHASE_APPLIED) {
        state->restore_phase = SESSION_RESTORE_PHASE_NONE;
        state->restore_mask = 0u;
        if (state->last_rx_kind == SESSION_REQUEST_RESTORE) {
            session_clear_duplicate(state);
        }
    }
}

uint8_t session_build_restore_chunk(const SessionWorkspace *workspace,
                                    uint8_t chunk,
                                    uint8_t *payload,
                                    uint8_t capacity)
{
    uint8_t offset = chunk == 0u ? 0u : 30u;

    if (payload == 0 || capacity < 36u) {
        return 0u;
    }
    payload[0] = 'R';
    payload[1] = 'S';
    payload[2] = '0';
    payload[3] = (uint8_t)('0' + chunk);
    payload[4] = ' ';
    memcpy(payload + 5u, workspace->restore + offset, 30u);
    payload[35] = 0u;
    return 1u;
}

uint8_t session_restore_chunk_matches(const SessionWorkspace *workspace,
                                      const uint8_t *payload,
                                      uint8_t chunk)
{
    uint8_t offset = chunk == 0u ? 0u : 30u;

    return (uint8_t)(memcmp(workspace->restore + offset,
                            payload + 5u, 30u) == 0);
}

void session_store_restore_chunk(SessionWorkspace *workspace,
                                 const uint8_t *payload,
                                 uint8_t chunk)
{
    uint8_t offset = chunk == 0u ? 0u : 30u;

    memcpy(workspace->restore + offset, payload + 5u, 30u);
}

void session_reset(SessionState *state)
{
    if (state == 0) {
        return;
    }
    state->session_id = state->config.session_id;
    state->current_ply = 0u;
    state->pending_value = 0u;
    state->last_value = 0u;
    state->phase = SESSION_PHASE_IDLE;
    state->local_color = SESSION_COLOR_UNKNOWN;
    if (state->config.host_color != SESSION_COLOR_UNKNOWN) {
        state->local_color = state->config.role == SESSION_ROLE_HOST
                                 ? state->config.host_color
                                 : (uint8_t)(state->config.host_color ^ 1u);
    }
    state->deferred_decision = 0u;
    state->peer_ready = 0u;
    state->link_up = 0u;
    state->active_link = SESSION_LINK_NONE;
    state->tx_link = SESSION_LINK_NONE;
    state->pending_control = 0u;
    state->pending_origin = 0u;
    state->pending_request_id = 0u;
    state->pending_tx_id = 0u;
    state->pending_tx_kind = 0u;
    state->next_tx_id = 1u;
    state->control_retries = 0u;
    state->liveness_misses = 0u;
    state->timer_mask = 0u;
    state->last_rx_kind = 0u;
    state->last_result = 0u;
    state->delivery_id = 0u;
    state->restore_phase = 0u;
    state->restore_mask = 0u;
}

uint8_t session_init(SessionState *state, const SessionConfig *config)
{
    if (state == 0 || !session_config_valid(config)) {
        return 0u;
    }
    state->config = *config;
    session_reset(state);
    return 1u;
}

uint8_t session_end(SessionState *state,
                    SessionAction *actions,
                    uint8_t action_capacity,
                    uint8_t close_link)
{
    uint8_t timer_id;
    uint8_t needed = (uint8_t)(1u + (close_link != 0u));
    uint8_t count = 0u;
    uint8_t active_link = state->active_link;
    uint8_t candidate_link = SESSION_LINK_NONE;

    if (state->pending_tx_kind != 0u &&
        state->tx_link != SESSION_LINK_NONE &&
        state->tx_link != active_link) {
        candidate_link = state->tx_link;
        ++needed;
    }

    for (timer_id = 0u; timer_id < SESSION_TIMER_COUNT; ++timer_id) {
        if ((state->timer_mask & (uint8_t)(1u << timer_id)) != 0u) {
            ++needed;
        }
    }
    if (actions == 0 || action_capacity < needed) {
        return 0u;
    }
    for (timer_id = 0u; timer_id < SESSION_TIMER_COUNT; ++timer_id) {
        if ((state->timer_mask & (uint8_t)(1u << timer_id)) != 0u) {
            actions[count].type = SESSION_ACT_TIMER_CANCEL;
            actions[count].data.timer_cancel.timer_id = timer_id;
            ++count;
        }
    }
    if (close_link != 0u) {
        actions[count].type = SESSION_ACT_LINK_CLOSE;
        actions[count].data.link_close.link_id = active_link;
        ++count;
    }
    if (candidate_link != SESSION_LINK_NONE) {
        actions[count].type = SESSION_ACT_LINK_CLOSE;
        actions[count].data.link_close.link_id = candidate_link;
        ++count;
    }
    actions[count].type = SESSION_ACT_SESSION_CHANGED;
    actions[count].data.session.status = SESSION_CHANGED_ENDED;
    actions[count].data.session.end_reason = SESSION_END_REASON_TRANSPORT_LOST;
    ++count;
    session_reset(state);
    return count;
}

uint8_t session_step(SessionState *state,
                     const SessionEvent *event,
                     SessionWorkspace *workspace,
                     uint8_t *tx_scratch,
                     uint8_t tx_capacity,
                     SessionAction *actions,
                     uint8_t action_capacity)
{
    (void)workspace;
    (void)tx_scratch;
    (void)tx_capacity;

    if (state == 0 || event == 0 || event->type < SESSION_EV_LINK_UP ||
        event->type > SESSION_EV_GAME_RESULT) {
        return 0u;
    }
    if (event->type == SESSION_EV_LINK_DOWN) {
        if (state->link_up != 0u &&
            event->data.link.link_id != state->active_link) {
            return 0u;
        }
        if (state->link_up == 0u && state->phase == SESSION_PHASE_IDLE &&
            state->timer_mask == 0u && state->pending_tx_kind == 0u) {
            return 0u;
        }
        return session_end(state, actions, action_capacity, 0u);
    }
    if (state->config.transport == SESSION_TRANSPORT_DIRECT) {
        return direct_session_step(state,
                                   event,
                                   workspace,
                                   tx_scratch,
                                   tx_capacity,
                                   actions,
                                   action_capacity);
    }
    return mqtt_session_step(state,
                             event,
                             workspace,
                             tx_scratch,
                             tx_capacity,
                             actions,
                             action_capacity);
}

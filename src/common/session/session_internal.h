#ifndef NETCHESSZX_COMMON_SESSION_INTERNAL_H
#define NETCHESSZX_COMMON_SESSION_INTERNAL_H

#include "common/session/session.h"

#define SESSION_RESTORE_PHASE_NONE 0u
#define SESSION_RESTORE_PHASE_APPLIED 4u

uint8_t session_text_length(const char *text);
uint8_t session_text_equal(const uint8_t *payload,
                           uint8_t length,
                           const char *text);
uint8_t session_text_prefix(const uint8_t *payload,
                            uint8_t length,
                            const char *prefix);
uint8_t session_slice_valid(const uint8_t *payload, uint8_t length);
uint8_t session_fixed_slice_valid(const uint8_t *payload, uint8_t length);
uint8_t session_parse_mach(const uint8_t *payload,
                           uint8_t length,
                           uint8_t *platform);
char *session_u16_text(char *out, uint16_t value);
uint16_t session_parse_u16(const char *text);
uint8_t session_emit_timer_cancel(SessionState *state,
                                  SessionAction *actions,
                                  uint8_t *count,
                                  uint8_t timer_id);
uint8_t session_emit_session(SessionAction *actions,
                             uint8_t *count,
                             uint8_t status);
uint8_t session_emit_end(SessionAction *actions,
                         uint8_t *count,
                         uint8_t end_reason);
uint8_t session_emit_side(SessionState *state,
                          SessionAction *actions,
                          uint8_t *count);
uint8_t session_emit_close(SessionAction *actions,
                           uint8_t *count,
                           uint8_t link_id);
uint8_t session_emit_game(SessionAction *actions,
                          uint8_t *count,
                          uint8_t kind,
                          uint8_t delivery_id,
                          uint16_t value,
                          const uint8_t *payload,
                          uint8_t length);
uint8_t session_emit_decision(SessionAction *actions,
                              uint8_t *count,
                              uint8_t request_id,
                              uint8_t control,
                              uint16_t value);

uint8_t session_next_tx_id(SessionState *state);
uint8_t session_next_delivery_id(SessionState *state);
void session_clear_duplicate(SessionState *state);
void session_drop_restore_cache(SessionState *state);
uint8_t session_build_restore_chunk(const SessionWorkspace *workspace,
                                    uint8_t chunk,
                                    uint8_t *payload,
                                    uint8_t capacity);
uint8_t session_restore_chunk_matches(const SessionWorkspace *workspace,
                                      const uint8_t *payload,
                                      uint8_t chunk);
void session_store_restore_chunk(SessionWorkspace *workspace,
                                 const uint8_t *payload,
                                 uint8_t chunk);

uint8_t session_end(SessionState *state,
                    SessionAction *actions,
                    uint8_t action_capacity,
                    uint8_t close_link);

uint8_t direct_session_step(SessionState *state,
                            const SessionEvent *event,
                            SessionWorkspace *workspace,
                            uint8_t *tx_scratch,
                            uint8_t tx_capacity,
                            SessionAction *actions,
                            uint8_t action_capacity);

uint8_t mqtt_session_step(SessionState *state,
                          const SessionEvent *event,
                          SessionWorkspace *workspace,
                          uint8_t *tx_scratch,
                          uint8_t tx_capacity,
                          SessionAction *actions,
                          uint8_t action_capacity);

#endif

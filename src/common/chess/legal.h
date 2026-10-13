#ifndef NETCHESSZX_LEGAL_H
#define NETCHESSZX_LEGAL_H

#include "common/chess/position.h"
#include "common/chess/action.h"

#ifdef __cplusplus
extern "C" {
#endif

int netchesszx_rules_reset(void);
int netchesszx_rules_can_play(const char *move);
int netchesszx_rules_play(const char *move);
/* MOVE's coordinate token only; framing, ply and notation remain protocol-owned.
   Decode normalizes promotion case. Failure leaves the output untouched. */
int netchesszx_action_decode(const char *text, netchesszx_action_t *action);
int netchesszx_action_encode(const netchesszx_action_t *action, char *out, size_t cap);
/* Validates shape, promotion and legality before any state mutation. */
int netchesszx_rules_apply_action(const netchesszx_action_t *action);
uint8_t netchesszx_rules_check_state(void);
int netchesszx_rules_has_legal_moves(void);
size_t netchesszx_rules_state_size(void);
#ifndef NETCHESSZX_FIXED_LOW_RAM
int netchesszx_rules_save(void *state, size_t cap);
int netchesszx_rules_restore(const void *state, size_t cap);
#endif
int netchesszx_rules_legal_targets(const char *from, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif

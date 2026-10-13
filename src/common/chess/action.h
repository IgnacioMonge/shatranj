#ifndef NETCHESSZX_CHESS_ACTION_H
#define NETCHESSZX_CHESS_ACTION_H

#include <stdint.h>

/* Chess-owned value, not a wire/save layout. Squares are rank-8-first 0..63;
   promo is zero or lowercase q/r/b/n. No pointers or retained storage. */
typedef struct {
    uint8_t from;
    uint8_t to;
    char promo;
} netchesszx_action_t;

#endif

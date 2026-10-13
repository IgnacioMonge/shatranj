#ifndef SPACE_DEMO_H
#define SPACE_DEMO_H

#include "common/space/space.h"

/* Bounded demonstration, not a new engine-wide board/state format. */
typedef struct {
    space_cell_t cell;
} demo_action_t;

void demo_reset(uint8_t hexagonal);
uint8_t demo_hexagonal(void);
uint16_t demo_extent(void);
uint8_t demo_valid(space_cell_t cell);
space_cell_t demo_first(void);
uint8_t demo_view(space_cell_t cell, uint8_t player);
uint8_t demo_activate(space_cell_t cell, uint8_t player);
/* The model validates the action and participant before mutating reveals. */
uint8_t demo_apply_action(const demo_action_t *action, uint8_t player);
uint8_t demo_x(space_cell_t cell);
uint8_t demo_y(space_cell_t cell);

#endif

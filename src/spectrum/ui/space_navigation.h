#ifndef SPECTRUM_SPACE_NAVIGATION_H
#define SPECTRUM_SPACE_NAVIGATION_H

#include "common/space/space.h"

/* The selected presentation maps an opaque position and visual input to
   another position. It owns orientation and geometry, never cursor storage.
   Unknown keys and outer edges retain the position; invalid IDs return NONE.
   Shatranj uses its existing row-low/column-high IDs at this boundary. */
space_cell_t spectrum_space_neighbor(space_cell_t position, uint8_t key);

#endif

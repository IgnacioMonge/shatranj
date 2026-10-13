#ifndef NETCHESSZX_SPECTRUM_UI_PRESENTATION_H
#define NETCHESSZX_SPECTRUM_UI_PRESENTATION_H

/* Expand the target's existing calls at their original call sites: no extra
 * resident entry, stack frame, or bank crossing. */
#ifdef NETCHESSZX_NEXT
#include "spectrum/ui/presentation_next.h"
#else
#include "spectrum/ui/presentation_zx.h"
#endif

#endif

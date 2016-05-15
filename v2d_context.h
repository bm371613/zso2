#ifndef V2D_CONTEXT_H
#define V2D_CONTEXT_H

#include "common.h"

int
v2d_context_initialize(v2d_context_t *ctx, uint16_t width, uint16_t height);

void
v2d_context_finalize(v2d_context_t *ctx);

#endif


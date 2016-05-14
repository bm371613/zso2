#ifndef V2D_BACKEND_H
#define V2D_BACKEND_H

#include <linux/interrupt.h>

#include "v2d_context.h"

typedef unsigned v2d_cmd_t;

void tear_down_ctx(v2d_device_t *dev);

int handle_cmd(v2d_context_t *ctx, v2d_cmd_t cmd);

irqreturn_t irq_handler(int irq, void *dev);

#endif


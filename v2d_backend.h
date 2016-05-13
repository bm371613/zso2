#ifndef V2D_BACKEND_H
#define V2D_BACKEND_H

#include <linux/interrupt.h>

#include "v2d_context.h"

typedef unsigned v2d_cmd_t;

irqreturn_t v2d_irq_handler(int irq, void *dev);

void v2d_handle_cmd(v2d_context_t *ctx, v2d_cmd_t command);
#endif


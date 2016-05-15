#ifndef V2D_BACKEND_H
#define V2D_BACKEND_H

#include <linux/interrupt.h>

#include "v2d_context.h"

void device_prepare(v2d_device_t *dev);
void device_reset(v2d_device_t *dev);

int handle_prepare_cmd(v2d_context_t *ctx, v2d_cmd_t cmd);
int handle_do_cmd(v2d_context_t *ctx, v2d_cmd_t cmd);

void sync_ctx(v2d_context_t *ctx);

irqreturn_t irq_handler(int irq, void *dev);

#endif


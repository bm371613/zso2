#include "v2d_backend.h"

/* helpers */

void switch_ctx(v2d_context_t *ctx) {
	v2d_device_t *dev = ctx->dev;
	v2d_context_t *current_ctx = dev->ctx;

	if (ctx == current_ctx)
		return;

	if (current_ctx != NULL) {
		// TODO tear down current_ctx
	}

	// TODO set up ctx
}


/* interface */

irqreturn_t v2d_irq_handler(int irq, void *dev) {
	// TODO
	return IRQ_HANDLED;
}

void v2d_handle_cmd(v2d_context_t *ctx, v2d_cmd_t command) {
	v2d_device_t *dev = ctx->dev;

	mutex_lock(&dev->mutex);
	switch_ctx(ctx);
	// TODO handle command
	mutex_unlock(&dev->mutex);
}

#include "v2d_backend.h"

/* helpers */

void set_up_ctx(v2d_context_t *ctx) {
	// TODO
}

/* interface */

void tear_down_ctx(v2d_device_t *dev) {
	// TODO
}

void handle_cmd(v2d_context_t *ctx, v2d_cmd_t command) {
	v2d_device_t *dev = ctx->dev;
	v2d_context_t *current_ctx;

	current_ctx = dev->ctx;
	if (ctx != current_ctx) {
		if (current_ctx != NULL)
			tear_down_ctx(dev);
		set_up_ctx(ctx);
	}

	// TODO handle command
}

irqreturn_t irq_handler(int irq, void *dev) {
	// TODO
	return IRQ_HANDLED;
}

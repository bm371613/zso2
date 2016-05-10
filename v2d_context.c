#include "v2d_context.h"

v2d_context_t *
v2d_context_create(struct pci_dev *dev)
{
	v2d_context_t *ctx = kmalloc(sizeof(v2d_context_t), GFP_KERNEL);
	if (!ctx)
		return ctx;

	ctx->dev = dev;
	ctx->width = 0;
	ctx->height = 0;
	ctx->ptable = NULL;

	dev_info(&(ctx->dev->dev), "Device context created");

	return ctx;
}

bool
v2d_context_is_initialized(v2d_context_t *ctx)
{
	return ctx->ptable != NULL;
}

int
v2d_context_initialize(v2d_context_t *ctx, uint16_t width, uint16_t height)
{
	if (v2d_context_is_initialized(ctx)
			|| MIN_CANVAS_SIZE > width
			|| MIN_CANVAS_SIZE > height
			|| MAX_CANVAS_SIZE < width
			|| MAX_CANVAS_SIZE < height)
		return -EINVAL;

	ctx->width = width;
	ctx->height = height;

	// TODO set up ptable

	dev_info(&(ctx->dev->dev), "Device context initialized (%d, %d)",
			width, height);

	return 0;
}

void
v2d_context_discard(v2d_context_t *ctx)
{
	if (v2d_context_is_initialized(ctx)) {
		// TODO clean up ptable
	}
	dev_info(&(ctx->dev->dev), "Device context discarded");
	kfree(ctx);
}

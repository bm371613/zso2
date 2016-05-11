#include "v2d_context.h"

/* helpers*/
int
dma_addr_mapping_initialize(dma_addr_mapping_t *dam, v2d_context_t *ctx)
{
	dam->addr = dma_alloc_coherent(&(ctx->v2d_dev->dev->dev),
			VINTAGE2D_PAGE_SIZE, &dam->dma_handle, GFP_KERNEL);
	if (!dam->addr) {
		dam->dma_handle = 0;
		return -1;
	}
	memset(dam->addr, 0, VINTAGE2D_PAGE_SIZE);
	return 0;
}

void
dma_addr_mapping_finalize(dma_addr_mapping_t *dam, v2d_context_t *ctx)
{
	dma_free_coherent(&(ctx->v2d_dev->dev->dev), VINTAGE2D_PAGE_SIZE,
			dam->addr, dam->dma_handle);
}

/* interface */

v2d_context_t *
v2d_context_create(v2d_device_t *v2d_dev)
{
	v2d_context_t *ctx = kmalloc(sizeof(v2d_context_t), GFP_KERNEL);
	if (!ctx)
		return ctx;

	ctx->v2d_dev = v2d_dev;
	ctx->canvas_pages_count = 0;

	dev_info(LOG_DEV(ctx), "context created");

	return ctx;
}

bool
v2d_context_is_initialized(v2d_context_t *ctx)
{
	return ctx->canvas_pages_count > 0;
}

int
v2d_context_initialize(v2d_context_t *ctx, uint16_t width, uint16_t height)
{
	int i;
	unsigned *page_table;

	if (v2d_context_is_initialized(ctx)
			|| MIN_CANVAS_SIZE > width
			|| MIN_CANVAS_SIZE > height
			|| MAX_CANVAS_SIZE < width
			|| MAX_CANVAS_SIZE < height)
		return -EINVAL;

	ctx->width = width;
	ctx->height = height;

	ctx->canvas_pages_count = DIV_ROUND_UP(width * height,
			VINTAGE2D_PAGE_SIZE);
	ctx->canvas_pages = kmalloc(
			ctx->canvas_pages_count * sizeof(dma_addr_mapping_t),
			GFP_KERNEL);

	for (i = 0; i < ctx->canvas_pages_count; ++i) {
		if(dma_addr_mapping_initialize(&ctx->canvas_pages[i], ctx))
			goto canvas_failure;
	}
	if (dma_addr_mapping_initialize(&ctx->canvas_page_table, ctx))
		goto canvas_failure;

	page_table = (unsigned *) ctx->canvas_page_table.addr;
	for (i = 0; i < ctx->canvas_pages_count; ++i)
		page_table[i] = VINTAGE2D_PTE_VALID
			| ctx->canvas_pages[i].dma_handle;

	dev_info(LOG_DEV(ctx), "context initialized (%d, %d)", width, height);

	return 0;

canvas_failure:
	while(i--)
		dma_addr_mapping_finalize(&ctx->canvas_pages[i], ctx);
	kfree(ctx->canvas_pages);
	ctx->canvas_pages_count = 0;
	return -ENOMEM;
}

void
v2d_context_discard(v2d_context_t *ctx)
{
	int i;
	if (v2d_context_is_initialized(ctx)) {
		for (i = 0; i < ctx->canvas_pages_count; ++i)
			dma_addr_mapping_finalize(&ctx->canvas_pages[i], ctx);
		dma_addr_mapping_finalize(&ctx->canvas_page_table, ctx);
		kfree(ctx->canvas_pages);
		ctx->canvas_pages_count = 0;
	}
	dev_info(LOG_DEV(ctx), "context discarded");
	kfree(ctx);
}

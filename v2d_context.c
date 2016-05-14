#include "v2d_context.h"

/* helpers*/
int
dma_addr_mapping_initialize(dma_addr_mapping_t *dam, v2d_context_t *ctx)
{
	dam->addr = dma_alloc_coherent(&(ctx->dev->dev->dev),
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
	dma_free_coherent(&(ctx->dev->dev->dev), VINTAGE2D_PAGE_SIZE,
			dam->addr, dam->dma_handle);
}

/* interface */
int
v2d_context_set_up_canvas(v2d_context_t *ctx, uint16_t width, uint16_t height)
{
	int i;
	unsigned *page_table;

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

	return 0;

canvas_failure:
	while(i--)
		dma_addr_mapping_finalize(&ctx->canvas_pages[i], ctx);
	kfree(ctx->canvas_pages);
	ctx->canvas_pages_count = 0;
	return -ENOMEM;
}

void
v2d_context_tear_down_canvas(v2d_context_t *ctx)
{
	int i;
	if (ctx->canvas_pages_count > 0) {
		for (i = 0; i < ctx->canvas_pages_count; ++i)
			dma_addr_mapping_finalize(&ctx->canvas_pages[i], ctx);
		dma_addr_mapping_finalize(&ctx->canvas_page_table, ctx);
		kfree(ctx->canvas_pages);
		ctx->canvas_pages_count = 0;
	}
	ctx->canvas_pages_count = 0;
}

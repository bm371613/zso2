#include "v2d_context.h"

int
v2d_context_initialize(v2d_context_t *ctx, uint16_t width, uint16_t height)
{
	int i;
	unsigned *page_table;

	ctx->width = width;
	ctx->height = height;
	ctx->history[0] = ctx->history[1] = 0;
	ctx->history_it = 0;

	ctx->canvas_pages_count = DIV_ROUND_UP(width * height,
			VINTAGE2D_PAGE_SIZE);
	ctx->canvas_pages = kmalloc(
			ctx->canvas_pages_count * sizeof(dma_addr_mapping_t),
			GFP_KERNEL);

	for (i = 0; i < ctx->canvas_pages_count; ++i) {
		if(dma_addr_mapping_initialize(&ctx->canvas_pages[i],
					ctx->dev))
			goto outcanvas;
	}
	if (dma_addr_mapping_initialize(&ctx->canvas_page_table, ctx->dev))
		goto outcanvas;

	page_table = (unsigned *) ctx->canvas_page_table.addr;
	for (i = 0; i < ctx->canvas_pages_count; ++i)
		page_table[i] = VINTAGE2D_PTE_VALID
			| ctx->canvas_pages[i].dma_handle;

	return 0;

outcanvas:
	while(i--)
		dma_addr_mapping_finalize(&ctx->canvas_pages[i], ctx->dev);
	kfree(ctx->canvas_pages);
	ctx->canvas_pages_count = 0;
	return -ENOMEM;
}

void
v2d_context_finalize(v2d_context_t *ctx)
{
	int i;
	if (ctx->canvas_pages_count > 0) {
		for (i = 0; i < ctx->canvas_pages_count; ++i)
			dma_addr_mapping_finalize(&ctx->canvas_pages[i],
					ctx->dev);
		dma_addr_mapping_finalize(&ctx->canvas_page_table, ctx->dev);
		kfree(ctx->canvas_pages);
		ctx->canvas_pages_count = 0;
	}
	ctx->canvas_pages_count = 0;
}

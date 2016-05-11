#ifndef V2D_CONTEXT_H
#define V2D_CONTEXT_H

#include <linux/pci.h>

#include "vintage2d.h"
#include "v2d_device.h"

#define MIN_CANVAS_SIZE 1
#define MAX_CANVAS_SIZE 2048
#define PTABLE_TOC_SIZE \
	(MAX_CANVAS_SIZE * MAX_CANVAS_SIZE / VINTAGE2D_PAGE_SIZE)

#define LOG_DEV(ctx) (&((ctx)->v2d_dev->dev->dev))

typedef struct {
	void *addr;
	dma_addr_t dma_handle;
} dma_addr_mapping_t;

typedef struct {
	v2d_device_t *v2d_dev;
	uint16_t width;
	uint16_t height;
	int canvas_pages_count;
	dma_addr_mapping_t canvas_page_table;
	dma_addr_mapping_t *canvas_pages;
} v2d_context_t;

v2d_context_t *
v2d_context_create(v2d_device_t *v2d_dev);

bool
v2d_context_is_initialized(v2d_context_t *ctx);

int
v2d_context_initialize(v2d_context_t *ctx, uint16_t width, uint16_t height);

void
v2d_context_discard(v2d_context_t *ctx);
#endif


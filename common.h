#ifndef COMMON_H
#define COMMON_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/pci.h>

#include "v2d_ioctl.h"
#include "vintage2d.h"

#define MIN_CANVAS_SIZE 1
#define MAX_CANVAS_SIZE 2048
#define MAX_CMDS (VINTAGE2D_PAGE_SIZE / 4)
#define PTABLE_TOC_SIZE \
	(MAX_CANVAS_SIZE * MAX_CANVAS_SIZE / VINTAGE2D_PAGE_SIZE)

#define DEV_CMDS_ADDR(v2d_dev) ((unsigned*) v2d_dev->cmds.addr)
#define DEV_CMDS_DMA(v2d_dev) ((unsigned) v2d_dev->cmds.dma_handle)
#define LOG_DEV(ctx) (&((ctx)->dev->dev->dev))

typedef unsigned v2d_cmd_t;

typedef struct {
	void *addr;
	dma_addr_t dma_handle;
} dma_addr_mapping_t;

struct v2d_context;

typedef struct {
	struct mutex mutex;

	struct v2d_context *ctx;

	int minor;
	struct pci_dev *dev;
	struct cdev *cdev;
	void __iomem *control;

	dma_addr_mapping_t cmds;
} v2d_device_t;

typedef struct v2d_context {
	struct mutex mutex;

	v2d_device_t *dev;

	uint16_t width;
	uint16_t height;
	int canvas_pages_count;
	dma_addr_mapping_t canvas_page_table;
	dma_addr_mapping_t *canvas_pages;

	v2d_cmd_t history[2];
	int history_it;
} v2d_context_t;

int
dma_addr_mapping_initialize(dma_addr_mapping_t *dam, v2d_device_t *dev);
void
dma_addr_mapping_finalize(dma_addr_mapping_t *dam, v2d_device_t *dev);

#endif

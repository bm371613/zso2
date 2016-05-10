#ifndef V2D_CONTEXT_H
#define V2D_CONTEXT_H

#include <linux/pci.h>

#define MIN_CANVAS_SIZE 1
#define MAX_CANVAS_SIZE 2048

typedef struct {
	struct pci_dev *dev;
	uint16_t width;
	uint16_t height;
	void *ptable;
} v2d_context_t;

v2d_context_t *
v2d_context_create(struct pci_dev *dev);

bool
v2d_context_is_initialized(v2d_context_t *ctx);

int
v2d_context_initialize(v2d_context_t *ctx, uint16_t width, uint16_t height);

void
v2d_context_discard(v2d_context_t *ctx);
#endif


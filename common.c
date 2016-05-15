#include "common.h"

int
dma_addr_mapping_initialize(dma_addr_mapping_t *dam, v2d_device_t *dev)
{
	dam->addr = dma_alloc_coherent(&(dev->dev->dev),
			VINTAGE2D_PAGE_SIZE, &dam->dma_handle, GFP_KERNEL);
	if (!dam->addr) {
		dam->dma_handle = 0;
		return -1;
	}
	memset(dam->addr, 0, VINTAGE2D_PAGE_SIZE);
	return 0;
}

void
dma_addr_mapping_finalize(dma_addr_mapping_t *dam, v2d_device_t *dev)
{
	dma_free_coherent(&(dev->dev->dev), VINTAGE2D_PAGE_SIZE,
			dam->addr, dam->dma_handle);
}

#include "v2d_backend.h"
#include "v2d_ioctl.h"

/* helpers */

inline unsigned get_registry(v2d_device_t *dev, unsigned offset)
{
	return *((unsigned *) (dev->control + offset));
}

inline void set_registry(v2d_device_t *dev, unsigned offset, unsigned value)
{
	*((unsigned *) (dev->control + offset)) = value;
}


/* interface */
void device_prepare(v2d_device_t *dev)
{
}

void device_reset(v2d_device_t *dev)
{
}


int handle_prepare_cmd(v2d_context_t *ctx, v2d_cmd_t cmd)
{
	return 0;
}

int handle_do_cmd(v2d_context_t *ctx, v2d_cmd_t cmd)
{
	return 0;
}

void sync_ctx(v2d_context_t *ctx)
{
}

irqreturn_t irq_handler(int irq, void *dev)
{
	v2d_device_t *v2d_dev = dev;
	unsigned intr = get_registry(dev, VINTAGE2D_INTR);


	if (intr & VINTAGE2D_INTR_NOTIFY)
		printk("v2d: irq notify\n");

	if (intr & VINTAGE2D_INTR_INVALID_CMD)
		printk("v2d: irq invalid command\n");

	if (intr & VINTAGE2D_INTR_PAGE_FAULT)
		printk("v2d: irq page fault\n");

	if (intr & VINTAGE2D_INTR_CANVAS_OVERFLOW)
		printk("v2d: irq canvas overflow\n");

	if (intr & VINTAGE2D_INTR_FIFO_OVERFLOW)
		printk("v2d: irq fifo overflow\n");

	set_registry(dev, VINTAGE2D_INTR, VINTAGE2D_INTR_NOTIFY
			| VINTAGE2D_INTR_INVALID_CMD
			| VINTAGE2D_INTR_PAGE_FAULT
			| VINTAGE2D_INTR_CANVAS_OVERFLOW
			| VINTAGE2D_INTR_FIFO_OVERFLOW);

	return IRQ_HANDLED;
}

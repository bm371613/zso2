#include "v2d_backend.h"
#include "v2d_ioctl.h"

/* helpers */

inline unsigned get_registry(v2d_device_t *dev, unsigned offset) {
	return *((unsigned *) (dev->control + offset));
}

inline void set_registry(v2d_device_t *dev, unsigned offset, unsigned value) {
	*((unsigned *) (dev->control + offset)) = value;
}

void set_up_ctx(v2d_context_t *ctx) {
	v2d_device_t *dev = ctx->dev;

	set_registry(dev, VINTAGE2D_ENABLE, VINTAGE2D_ENABLE_DRAW);
	set_registry(dev, VINTAGE2D_INTR_ENABLE, VINTAGE2D_INTR_NOTIFY
			| VINTAGE2D_INTR_INVALID_CMD
			| VINTAGE2D_INTR_PAGE_FAULT
			| VINTAGE2D_INTR_CANVAS_OVERFLOW
			| VINTAGE2D_INTR_FIFO_OVERFLOW);

	set_registry(dev, VINTAGE2D_FIFO_SEND, VINTAGE2D_CMD_CANVAS_PT(
				ctx->canvas_page_table.dma_handle, 0));
	set_registry(dev, VINTAGE2D_FIFO_SEND, VINTAGE2D_CMD_CANVAS_DIMS(
				ctx->width, ctx->height, 0));
	dev->ctx = ctx;
}

/* interface */

void tear_down_ctx(v2d_device_t *dev) {
	set_registry(dev, VINTAGE2D_ENABLE, 0);
	set_registry(dev, VINTAGE2D_INTR_ENABLE, 0);
	set_registry(dev, VINTAGE2D_RESET, VINTAGE2D_RESET_DRAW
			| VINTAGE2D_RESET_FIFO | VINTAGE2D_RESET_TLB);
	set_registry(dev, VINTAGE2D_INTR, VINTAGE2D_INTR_NOTIFY
			| VINTAGE2D_INTR_INVALID_CMD
			| VINTAGE2D_INTR_PAGE_FAULT
			| VINTAGE2D_INTR_CANVAS_OVERFLOW
			| VINTAGE2D_INTR_FIFO_OVERFLOW);
	dev->ctx = NULL;
}

int handle_cmd(v2d_context_t *ctx, v2d_cmd_t cmd) {
	v2d_device_t *dev = ctx->dev;
	v2d_context_t *current_ctx = dev->ctx;

	// TODO validate commands

	switch (V2D_CMD_TYPE(cmd)) {
	case V2D_CMD_TYPE_SRC_POS:
		ctx->src_x = V2D_CMD_POS_X(cmd);
		ctx->src_y = V2D_CMD_POS_Y(cmd);
		break;
	case V2D_CMD_TYPE_DST_POS:
		ctx->dst_x = V2D_CMD_POS_X(cmd);
		ctx->dst_y = V2D_CMD_POS_Y(cmd);
		break;
	case V2D_CMD_TYPE_FILL_COLOR:
		ctx->color = V2D_CMD_COLOR(cmd);
		break;
	case V2D_CMD_TYPE_DO_FILL:
		if (ctx != current_ctx) {
			tear_down_ctx(dev);
			set_up_ctx(ctx);
		}
		set_registry(dev, VINTAGE2D_FIFO_SEND,
				VINTAGE2D_CMD_FILL_COLOR(ctx->color, 0));
		set_registry(dev, VINTAGE2D_FIFO_SEND,
				VINTAGE2D_CMD_DST_POS(
					ctx->dst_x, ctx->dst_y, 0));
		set_registry(dev, VINTAGE2D_FIFO_SEND,
				VINTAGE2D_CMD_DO_FILL(
					V2D_CMD_WIDTH(cmd),
					V2D_CMD_HEIGHT(cmd),
					1));
		printk("cmd\n");
		wait_for_completion(&ctx->completion);
		printk("cmd end!\n");
		break;
	case V2D_CMD_TYPE_DO_BLIT:
		if (ctx != current_ctx) {
			tear_down_ctx(dev);
			set_up_ctx(ctx);
		}
		set_registry(dev, VINTAGE2D_FIFO_SEND,
				VINTAGE2D_CMD_SRC_POS(
					ctx->src_x, ctx->src_y, 0));
		set_registry(dev, VINTAGE2D_FIFO_SEND,
				VINTAGE2D_CMD_DST_POS(
					ctx->dst_x, ctx->dst_y, 0));
		set_registry(dev, VINTAGE2D_FIFO_SEND,
				VINTAGE2D_CMD_DO_BLIT(
					V2D_CMD_WIDTH(cmd),
					V2D_CMD_HEIGHT(cmd),
					1));
		printk("cmd\n");
		wait_for_completion(&ctx->completion);
		printk("cmd end!\n");
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

irqreturn_t irq_handler(int irq, void *dev) {
	v2d_device_t *v2d_dev = dev;
	v2d_context_t *ctx = v2d_dev->ctx;
	unsigned intr = get_registry(dev, VINTAGE2D_INTR);

	printk("irq %p\n", ctx);

	if (ctx)
		complete(&ctx->completion);

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

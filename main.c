#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <asm/uaccess.h>

#include "common.h"
#include "v2d_device.h"
#include "v2d_context.h"

MODULE_LICENSE("GPL");

int max_devices = 256;
module_param(max_devices, int, 0);

static struct pci_device_id v2d_ids[] = {
	{ PCI_DEVICE(VINTAGE2D_VENDOR_ID, VINTAGE2D_DEVICE_ID), },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, v2d_ids);

static dev_t devno;
static struct class *class;
static v2d_device_t *devices;

/* helpers */
inline unsigned
get_registry(v2d_device_t *dev, unsigned offset)
{
	return ioread32(dev->control + offset);
}

inline void
set_registry(v2d_device_t *dev, unsigned offset, unsigned value)
{
	iowrite32(value, dev->control + offset);
}


void
device_prepare(v2d_device_t *dev)
{
	unsigned cmds = DEV_CMDS_DMA(dev);

	set_registry(dev, VINTAGE2D_RESET, VINTAGE2D_RESET_DRAW
			| VINTAGE2D_RESET_FIFO | VINTAGE2D_RESET_TLB);
	set_registry(dev, VINTAGE2D_INTR, VINTAGE2D_INTR_NOTIFY
			| VINTAGE2D_INTR_INVALID_CMD
			| VINTAGE2D_INTR_PAGE_FAULT
			| VINTAGE2D_INTR_CANVAS_OVERFLOW
			| VINTAGE2D_INTR_FIFO_OVERFLOW);


	DEV_CMDS_ADDR(dev)[CMDS_SIZE - 1] = (cmds & (0xfffffffc)) | 2;
	set_registry(dev, VINTAGE2D_CMD_READ_PTR, (unsigned) cmds);
	set_registry(dev, VINTAGE2D_CMD_WRITE_PTR, (unsigned) cmds);

	set_registry(dev, VINTAGE2D_INTR_ENABLE, VINTAGE2D_INTR_NOTIFY
		| VINTAGE2D_INTR_INVALID_CMD
		| VINTAGE2D_INTR_PAGE_FAULT
		| VINTAGE2D_INTR_CANVAS_OVERFLOW
		| VINTAGE2D_INTR_FIFO_OVERFLOW);
	set_registry(dev, VINTAGE2D_ENABLE, VINTAGE2D_ENABLE_DRAW
		| VINTAGE2D_ENABLE_FETCH_CMD);
}

void
device_reset(v2d_device_t *dev)
{
	set_registry(dev, VINTAGE2D_ENABLE, 0);
	set_registry(dev, VINTAGE2D_INTR_ENABLE, 0);
	set_registry(dev, VINTAGE2D_RESET, VINTAGE2D_RESET_DRAW
			| VINTAGE2D_RESET_FIFO | VINTAGE2D_RESET_TLB);
	set_registry(dev, VINTAGE2D_INTR, VINTAGE2D_INTR_NOTIFY
			| VINTAGE2D_INTR_INVALID_CMD
			| VINTAGE2D_INTR_PAGE_FAULT
			| VINTAGE2D_INTR_CANVAS_OVERFLOW
			| VINTAGE2D_INTR_FIFO_OVERFLOW);
}

int
cmds_count(v2d_device_t *dev)
{
	unsigned r = (get_registry(dev, VINTAGE2D_CMD_READ_PTR)
			- DEV_CMDS_DMA(dev)) / 4,
		 w = (get_registry(dev, VINTAGE2D_CMD_WRITE_PTR)
			- DEV_CMDS_DMA(dev)) / 4;
	return r <= w ? w - r : w + (CMDS_SIZE - 1 - r);
}

void
send_encoded_cmd(v2d_device_t *dev, unsigned cmd)
{
	unsigned pos;

	wait_event(dev->queue, cmds_count(dev) + 1 < CMDS_SIZE - 1);

	pos = (get_registry(dev, VINTAGE2D_CMD_WRITE_PTR)
			- DEV_CMDS_DMA(dev)) / 4;
	DEV_CMDS_ADDR(dev)[pos] = cmd;
	pos++;
	if (pos == CMDS_SIZE - 1)
		pos = 0;

	set_registry(dev, VINTAGE2D_CMD_WRITE_PTR,
			DEV_CMDS_DMA(dev) + 4 * pos);
}

void
set_context(v2d_context_t *ctx)
{
	v2d_device_t *dev = ctx->dev;
	set_registry(dev, VINTAGE2D_RESET, VINTAGE2D_RESET_DRAW
			| VINTAGE2D_RESET_FIFO | VINTAGE2D_RESET_TLB);
	send_encoded_cmd(dev, VINTAGE2D_CMD_CANVAS_PT(
			ctx->canvas_page_table.dma_handle, 1));
	send_encoded_cmd(dev, VINTAGE2D_CMD_CANVAS_DIMS(
			ctx->width, ctx->height, 1));
	dev->ctx = ctx;
}

bool
validate_cmd(v2d_context_t *ctx, v2d_cmd_t cmd)
{
	unsigned color = V2D_CMD_COLOR(cmd),
		x = V2D_CMD_POS_X(cmd), y = V2D_CMD_POS_Y(cmd),
		width = V2D_CMD_WIDTH(cmd), height = V2D_CMD_HEIGHT(cmd);
	int i, src=-1, dst=-1, col=-1;

	for (i = 0; i < 2; ++i)
		switch (V2D_CMD_TYPE(ctx->history[i])) {
		case V2D_CMD_TYPE_SRC_POS:
			src = i;
			break;
		case V2D_CMD_TYPE_DST_POS:
			dst = i;
			break;
		case V2D_CMD_TYPE_FILL_COLOR:
			col = i;
			break;
		}


#define assert(cond) if(!(cond)) {return false;};
#define H(i) (ctx->history[i])
	switch (V2D_CMD_TYPE(cmd)) {
	case V2D_CMD_TYPE_SRC_POS:
		assert(cmd == V2D_CMD_SRC_POS(x, y));
		assert(x < ctx->width && y < ctx->height);
		break;
	case V2D_CMD_TYPE_DST_POS:
		assert(cmd == V2D_CMD_DST_POS(x, y));
		assert(x < ctx->width && y < ctx->height);
		break;
	case V2D_CMD_TYPE_FILL_COLOR:
		assert(cmd == V2D_CMD_FILL_COLOR(color));
		break;
	case V2D_CMD_TYPE_DO_FILL:
		assert(cmd == V2D_CMD_DO_FILL(width, height));
		assert(dst >= 0 && col >= 0);
		assert(V2D_CMD_POS_X(H(dst)) + width <= ctx->width
				&& V2D_CMD_POS_Y(H(dst)) + height
				<= ctx->height);
		break;
	case V2D_CMD_TYPE_DO_BLIT:
		assert(cmd == V2D_CMD_DO_BLIT(width, height));
		assert(dst >= 0 && src >= 0);
		assert(V2D_CMD_POS_X(H(dst)) + width <= ctx->width
				&& V2D_CMD_POS_Y(H(dst)) + height
				<= ctx->height);
		assert(V2D_CMD_POS_X(H(src)) + width <= ctx->width
				&& V2D_CMD_POS_Y(H(src)) + height
				<= ctx->height);
		break;
	default:
		return false;
	}
#undef assert
#undef H
	return true;
}

void
send_cmd(v2d_device_t *dev, v2d_cmd_t cmd)
{
	unsigned encoded_cmd;
	const int notify = 1;

	switch (V2D_CMD_TYPE(cmd)) {
	case V2D_CMD_TYPE_SRC_POS:
		encoded_cmd = VINTAGE2D_CMD_SRC_POS(
				V2D_CMD_POS_X(cmd),
				V2D_CMD_POS_Y(cmd),
				notify);
		break;
	case V2D_CMD_TYPE_DST_POS:
		encoded_cmd = VINTAGE2D_CMD_DST_POS(
				V2D_CMD_POS_X(cmd),
				V2D_CMD_POS_Y(cmd),
				notify);
		break;
	case V2D_CMD_TYPE_FILL_COLOR:
		encoded_cmd = VINTAGE2D_CMD_FILL_COLOR(
				V2D_CMD_COLOR(cmd),
				notify);
		break;
	case V2D_CMD_TYPE_DO_FILL:
		encoded_cmd = VINTAGE2D_CMD_DO_FILL(
				V2D_CMD_WIDTH(cmd),
				V2D_CMD_HEIGHT(cmd),
				notify);
		break;
	case V2D_CMD_TYPE_DO_BLIT:
		encoded_cmd = VINTAGE2D_CMD_DO_BLIT(
				V2D_CMD_WIDTH(cmd),
				V2D_CMD_HEIGHT(cmd),
				notify);
		break;
	default:
		return;
	}

	send_encoded_cmd(dev, encoded_cmd);
}

void
sync_device(v2d_device_t *dev)
{
	unsigned marker = get_registry(dev, VINTAGE2D_COUNTER) == 0 ? 1 : 0;

	send_encoded_cmd(dev, VINTAGE2D_CMD_COUNTER(marker, 1));
	wait_event(dev->queue, get_registry(dev, VINTAGE2D_COUNTER) == marker);
	dev->ctx = NULL;
}

irqreturn_t
irq_handler(int irq, void *dev)
{
	v2d_device_t *v2d_dev = dev;
	unsigned intr = get_registry(dev, VINTAGE2D_INTR);

	wake_up(&v2d_dev->queue);

	if (!(intr & VINTAGE2D_INTR_NOTIFY))
		printk("v2d: not irq notify\n");

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

/* vm */
void v2d_vm_open(struct vm_area_struct *vma)
{
	v2d_context_t *ctx = vma->vm_private_data;
	dev_info(LOG_DEV(ctx), "vm open %lx", vma->vm_pgoff);
}

void v2d_vm_close(struct vm_area_struct *vma)
{
	v2d_context_t *ctx = vma->vm_private_data;
	dev_info(LOG_DEV(ctx), "vm close");
}

int v2d_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	pgoff_t pgoff = vmf->pgoff;
	struct page *page;
	v2d_context_t *ctx = vma->vm_private_data;

	if (pgoff >= ctx->canvas_pages_count)
		return VM_FAULT_SIGBUS;
	page = pfn_to_page(__pa(ctx->canvas_pages[pgoff].addr) >> PAGE_SHIFT);
	if (!page) {
		dev_err(LOG_DEV(ctx), "pfn_to_page");
		return VM_FAULT_SIGBUS;
	}
	get_page(page);
	vmf->page = page;
	return 0;
}

static struct vm_operations_struct v2d_vm_ops = {
	.open = v2d_vm_open,
	.close = v2d_vm_close,
	.fault = v2d_vm_fault
};

/* file */

static int
v2d_open(struct inode *inode, struct file *file)
{
	v2d_device_t *dev = v2d_devices_by_minor(
		devices,
		max_devices,
		iminor(inode));
	v2d_context_t *ctx = kmalloc(sizeof(v2d_context_t), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mutex_init(&ctx->mutex);
	ctx->dev = dev;
	ctx->canvas_pages_count = 0;

	dev_info(LOG_DEV(ctx), "context created");

	file->private_data = (void*) ctx;
	return 0;
}

static int
v2d_release(struct inode *inode, struct file *file)
{
	v2d_context_t *ctx = file->private_data;
	v2d_device_t *dev = ctx->dev;

	mutex_lock(&dev->mutex);
	mutex_lock(&ctx->mutex);
	if (dev->ctx == ctx)
		sync_device(dev);
	v2d_context_finalize(ctx);
	ctx->canvas_pages_count = -1;
	dev_info(LOG_DEV(ctx), "context discarded");
	mutex_unlock(&ctx->mutex);
	mutex_unlock(&dev->mutex);

	kfree(ctx);
	return 0;
}

static long
v2d_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	v2d_context_t *ctx = file->private_data;
	struct v2d_ioctl_set_dimensions dim;
	long ret;

	if (cmd != V2D_IOCTL_SET_DIMENSIONS)
		return -ENOTTY;

	if (copy_from_user((void*) &dim, (void*) arg,
			sizeof(struct v2d_ioctl_set_dimensions)))
		return -EFAULT;

	mutex_lock(&ctx->mutex);
	if (ctx->canvas_pages_count != 0
			|| MIN_CANVAS_SIZE > dim.width
			|| MIN_CANVAS_SIZE > dim.height
			|| MAX_CANVAS_SIZE < dim.width
			|| MAX_CANVAS_SIZE < dim.height) {
		mutex_unlock(&ctx->mutex);
		return -EINVAL;
	}
	ret = v2d_context_initialize(ctx, dim.width, dim.height);
	if (ret == 0)
		dev_info(LOG_DEV(ctx), "context initialized (%d, %d)",
				dim.width, dim.height);
	mutex_unlock(&ctx->mutex);

	return ret;
}

static int
v2d_mmap(struct file *file, struct vm_area_struct *vma)
{
	v2d_context_t *ctx = file->private_data;

	mutex_lock(&ctx->mutex);
	if (ctx->canvas_pages_count <= 0) {
		mutex_unlock(&ctx->mutex);
		return -EINVAL;
	}
	vma->vm_private_data = file->private_data;
	vma->vm_ops = &v2d_vm_ops;
	mutex_unlock(&ctx->mutex);

	v2d_vm_open(vma);
	return 0;
}

static ssize_t
v2d_write(struct file *file, const char *buffer, size_t len, loff_t *off)
{
	v2d_cmd_t cmd;
	v2d_context_t *ctx = (v2d_context_t *) file->private_data;
	v2d_device_t *dev = ctx->dev;
	int i;

	if (len % 4)
		return -1;

	for (i = 0; i + 4 <= len; i = i + 4) {
		if (copy_from_user(&cmd, buffer + i, 4))
			return -EFAULT;
		switch (V2D_CMD_TYPE(cmd)) {
		case V2D_CMD_TYPE_SRC_POS:
		case V2D_CMD_TYPE_DST_POS:
		case V2D_CMD_TYPE_FILL_COLOR:
			mutex_lock(&ctx->mutex);
			if (ctx->canvas_pages_count <= 0) {
				mutex_unlock(&ctx->mutex);
				return -EINVAL;
			}
			if (!validate_cmd(ctx, cmd)) {
				mutex_unlock(&ctx->mutex);
				return -EINVAL;
			}
			ctx->history[ctx->history_it] = cmd;
			ctx->history_it = (ctx->history_it + 1) % 2;
			mutex_unlock(&ctx->mutex);
			break;
		case V2D_CMD_TYPE_DO_FILL:
		case V2D_CMD_TYPE_DO_BLIT:
			mutex_lock(&dev->mutex);
			mutex_lock(&ctx->mutex);
			if (ctx->canvas_pages_count <= 0) {
				mutex_unlock(&ctx->mutex);
				mutex_unlock(&dev->mutex);
				return -EINVAL;
			}
			if (dev->dev == NULL) {
				mutex_unlock(&ctx->mutex);
				mutex_unlock(&dev->mutex);
				return -ENODEV;
			}
			if (!validate_cmd(ctx, cmd)) {
				mutex_unlock(&ctx->mutex);
				mutex_unlock(&dev->mutex);
				return -EINVAL;
			}
			if (dev->ctx != ctx) {
				if (dev->ctx != NULL)
					sync_device(dev);
				set_context(ctx);
			}
			send_cmd(dev, ctx->history[0]);
			send_cmd(dev, ctx->history[1]);
			send_cmd(dev, cmd);
			ctx->history[ctx->history_it] = cmd;
			ctx->history_it = (ctx->history_it + 1) % 2;
			mutex_unlock(&ctx->mutex);
			mutex_unlock(&dev->mutex);
			break;
		default:
			return -EINVAL;
		}
	}
	return i;
}

static int
v2d_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	v2d_context_t *ctx = (v2d_context_t *) file->private_data;
	v2d_device_t *dev = ctx->dev;
	int ret = 0;

	mutex_lock(&dev->mutex);

	if (dev->dev == NULL) {
		ret = -ENODEV;
		goto out;
	}
	if (ctx->canvas_pages_count <= 0) {
		ret = -EINVAL;
		goto out;
	}

	sync_device(dev);
out:
	mutex_unlock(&dev->mutex);
	return ret;
}

static struct file_operations v2d_file_ops = {
	.owner		= THIS_MODULE,
	.open 		= v2d_open,
	.release 	= v2d_release,
	.unlocked_ioctl = v2d_ioctl,
	.mmap		= v2d_mmap,
	.write 		= v2d_write,
	.fsync		= v2d_fsync
};

/* pci */

static int
v2d_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int minor;
	struct cdev *cdev;
	struct device *device = NULL;
	v2d_device_t *v2d_dev;

	v2d_dev = v2d_devices_add(devices, max_devices, dev);
	if (v2d_dev == NULL) {
		dev_err(&(dev->dev), "v2d_devices_add");
		goto outadd;
	}
	v2d_dev->ctx = NULL;
	minor = v2d_dev->minor;

	cdev = cdev_alloc();
	cdev_init(cdev, &v2d_file_ops);
	cdev->owner = THIS_MODULE;
	if (cdev_add(cdev, MKDEV(MAJOR(devno), minor), 1) != 0) {
		dev_err(&(dev->dev), "cdev_add\n");
		goto outcdev;
	}
	v2d_dev->cdev = cdev;

	device = device_create(class, NULL, MKDEV(MAJOR(devno), minor), NULL,
			"v2d%d", minor);
	if (IS_ERR(device)) {
		dev_err(&(dev->dev), "device_create");
		goto outdevice;
	}

	if (pci_enable_device(dev)) {
		dev_err(&(dev->dev), "pci_enable_device");
		goto outenable;
	}

	if (IS_ERR_VALUE(pci_request_regions(dev, "v2d"))) {
		dev_err(&(dev->dev), "pci_request_regions");
		goto outregions;
	}

	v2d_dev->control = pci_iomap(dev, 0, 4096);
	if (!v2d_dev->control) {
		dev_err(&(dev->dev), "pci_iomap");
		goto outiomap;
	}

	if (request_irq(dev->irq, irq_handler, IRQF_SHARED, "v2d", v2d_dev)) {
		dev_err(&(dev->dev), "request_irq");
		goto outirq;
	}

	if (dma_addr_mapping_initialize(&v2d_dev->cmds, v2d_dev)) {
		dev_err(&(dev->dev), "dma_addr_mapping_initialize");
		goto outcmds;
	}

	pci_set_master(dev);
	pci_set_dma_mask(dev, DMA_BIT_MASK(32));
	pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(32));

	device_prepare(v2d_dev);

	dev_info(&(dev->dev), "registered %d", minor);

	return 0;

outcmds:
	free_irq(dev->irq, v2d_dev);
outirq:
	pci_iounmap(dev, v2d_dev->control);
outiomap:
	pci_release_regions(dev);
outregions:
	pci_disable_device(dev);
outenable:
	device_destroy(class, MKDEV(MAJOR(devno), v2d_dev->minor));
outdevice:
	cdev_del(cdev);
outcdev:
	v2d_devices_del(devices, max_devices, dev);
outadd:
	return -1;
}

static void
v2d_remove(struct pci_dev *dev)
{
	v2d_device_t *v2d_dev = v2d_devices_by_dev(devices, max_devices, dev);

	mutex_lock(&v2d_dev->mutex);
	device_reset(v2d_dev);
	dma_addr_mapping_finalize(&v2d_dev->cmds, v2d_dev);
	free_irq(dev->irq, v2d_dev);
	pci_iounmap(dev, v2d_dev->control);
	pci_release_regions(dev);
	pci_disable_device(dev);
	device_destroy(class, MKDEV(MAJOR(devno), v2d_dev->minor));
	cdev_del(v2d_dev->cdev);
	v2d_devices_del(devices, max_devices, dev);
	mutex_unlock(&v2d_dev->mutex);
}

static struct pci_driver v2d_pci_driver = {
	.name 		= "v2d",
	.id_table 	= v2d_ids,
	.probe 		= v2d_probe,
	.remove 	= v2d_remove,
};

/* module interface */

static int __init
v2d_init_module(void)
{
	devices = kmalloc(max_devices * sizeof(v2d_device_t),
			GFP_KERNEL);
	if (!devices) {
		printk(KERN_ERR "v2d: kmalloc\n");
		goto outalloc;
	}

	if (alloc_chrdev_region(&devno, 0, max_devices, "v2d") < 0) {
		printk(KERN_ERR "v2d: alloc_chrdev_region\n");
		goto outchrdev;
	}

	v2d_devices_init(devices, max_devices, MINOR(devno));

	class = class_create(THIS_MODULE, "v2d");
	if (IS_ERR(class)) {
		printk(KERN_ERR "v2d: class_create\n");
		goto outclass;
	}

	if (pci_register_driver(&v2d_pci_driver) < 0) {
		printk(KERN_ERR "v2d: pci_register_driver\n");
		goto outregister;
	}

	return 0;

outregister:
	class_destroy(class);
outclass:
	unregister_chrdev_region(devno, 1);
outchrdev:
	kfree(devices);
outalloc:
	return -1;
}

static void
v2d_exit_module(void)
{
	pci_unregister_driver(&v2d_pci_driver);
	class_destroy(class);
	unregister_chrdev_region(devno, max_devices);
	kfree(devices);
}

module_init(v2d_init_module);
module_exit(v2d_exit_module);


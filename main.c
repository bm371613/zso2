#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include "vintage2d.h"
#include "v2d_ioctl.h"
#include "v2d_device.h"
#include "v2d_context.h"
#include "v2d_backend.h"

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

	dev_info(LOG_DEV(ctx), "vm fault %lx", pgoff);

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
	v2d_context_t *ctx = v2d_context_create(dev);

	if (!ctx)
		return -ENOMEM;
	file->private_data = (void*) ctx;
	return 0;
}

static int
v2d_release(struct inode *inode, struct file *file)
{
	v2d_context_discard(file->private_data);
	return 0;
}

static long
v2d_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	v2d_context_t *ctx = file->private_data;
	struct v2d_ioctl_set_dimensions dim;

	switch (cmd) {
	case V2D_IOCTL_SET_DIMENSIONS:
		if (copy_from_user((void*) &dim, (void*) arg,
				sizeof(struct v2d_ioctl_set_dimensions)))
			return -EFAULT;
		return v2d_context_initialize(ctx, dim.width, dim.height);
	default:
		return -EINVAL;
	}
}

static int
v2d_mmap(struct file *file, struct vm_area_struct *vma)
{
	if (!v2d_context_is_initialized(file->private_data))
		return -EINVAL;
	vma->vm_private_data = file->private_data;
	vma->vm_ops = &v2d_vm_ops;
	v2d_vm_open(vma);
	return 0;
}

static ssize_t
v2d_write(struct file *file, const char *buffer, size_t len, loff_t *off)
{
	v2d_cmd_t cmd;
	v2d_context_t *ctx = (v2d_context_t *) file->private_data;
	int i;

	if (!v2d_context_is_initialized(ctx))
		return -EINVAL;

	for (i = 0; i + 4 <= len; i = i + 4) {
		if (copy_from_user(&cmd, buffer + i, 4))
			return -EFAULT;
		v2d_handle_cmd(ctx, cmd);
	}
	return i;

}

static int
v2d_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	return 0;
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

	if (request_irq(dev->irq, v2d_irq_handler, IRQF_SHARED, "v2d",
				v2d_dev)) {
		dev_err(&(dev->dev), "request_irq");
		goto outirq;
	}

	pci_set_master(dev);
	pci_set_dma_mask(dev, DMA_BIT_MASK(32));
	pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(32));

	dev_info(&(dev->dev), "registered %d", minor);

	return 0;

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

	free_irq(dev->irq, v2d_dev);
	pci_iounmap(dev, v2d_dev->control);
	pci_release_regions(dev);
	pci_disable_device(dev);
	device_destroy(class, MKDEV(MAJOR(devno), v2d_dev->minor));
	cdev_del(v2d_dev->cdev);
	v2d_devices_del(devices, max_devices, dev);
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
	int i;

	pci_unregister_driver(&v2d_pci_driver);
	for (i=0; i< max_devices; i++) {
		if (devices[i].dev != NULL) {
			cdev_del(devices[i].cdev);
		}
	}
	class_destroy(class);
	unregister_chrdev_region(devno, max_devices);
	kfree(devices);
}

module_init(v2d_init_module);
module_exit(v2d_exit_module);


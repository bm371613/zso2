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

/* chardev interface */

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
	return -1;
}

static ssize_t
v2d_write(struct file *file, const char *buffer, size_t len, loff_t *off)
{
	return len;
}

static int
v2d_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	return 0;
}

static struct file_operations pci_ops = {
	.owner		= THIS_MODULE,
	.open 		= v2d_open,
	.release 	= v2d_release,
	.unlocked_ioctl = v2d_ioctl,
	.mmap		= v2d_mmap,
	.write 		= v2d_write,
	.fsync		= v2d_fsync
};

/* pci interface */

static int
v2d_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int minor;
	struct cdev *cdev;
	struct device *device = NULL;
	v2d_device_t *v2d_dev;

	v2d_dev = v2d_devices_add(devices, max_devices, dev);
	if (v2d_dev == NULL) {
		dev_err(&(dev->dev), "v2d_devices_add\n");
		goto error;
	}
	minor = v2d_dev->minor;

	cdev = cdev_alloc();
	cdev_init(cdev, &pci_ops);
	cdev->owner = THIS_MODULE;
	if (cdev_add(cdev, MKDEV(MAJOR(devno), minor), 1) != 0) {
		dev_err(&(dev->dev), "cdev_add\n");
		goto error_pci_cdev;
	}
	v2d_dev->cdev = cdev;

	device = device_create(class, NULL, MKDEV(MAJOR(devno), minor), NULL,
			"v2d%d", minor);
	if (IS_ERR(device)) {
		dev_err(&(dev->dev), "device_create\n");
		goto error_cdev;
	}

	if (pci_enable_device(dev)) {
		dev_err(&(dev->dev), "pci_enable_device\n");
		goto error_cdev;
	}

	if (IS_ERR_VALUE(pci_request_regions(dev, "v2d"))) {
		dev_err(&(dev->dev), "pci_request_regions\n");
		goto error_pci_enable;
	}

	pci_set_master(dev);
	pci_set_dma_mask(dev, DMA_BIT_MASK(32));
	pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(32));

	v2d_dev->control = pci_iomap(dev, 0, 4096);

	dev_info(&(dev->dev), "Registered device %d", minor);

	return 0;

error_pci_enable:
	pci_disable_device(dev);
error_cdev:
	cdev_del(cdev);
error_pci_cdev:
	v2d_devices_del(devices, max_devices, dev);
error:
	return -1;
}

static void
v2d_remove(struct pci_dev *dev)
{
	v2d_device_t *v2d_dev = v2d_devices_by_dev(devices, max_devices, dev);

	pci_iounmap(dev, v2d_dev->control);
	pci_release_regions(dev);
	pci_disable_device(dev);

	device_destroy(class, MKDEV(MAJOR(devno), v2d_dev->minor));
	cdev_del(v2d_dev->cdev);
	v2d_devices_del(devices, max_devices, dev);
}

static struct pci_driver pci_driver = {
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
		goto error;
	}

	if (alloc_chrdev_region(&devno, 0, max_devices, "v2d") < 0) {
		printk(KERN_ERR "v2d: alloc_chrdev_region\n");
		goto error_kmalloc;
	}

	v2d_devices_init(devices, max_devices, MINOR(devno));

	class = class_create(THIS_MODULE, "v2d");
	if (IS_ERR(class)) {
		printk(KERN_ERR "v2d: class_create\n");
		goto error_chrdev;
	}

	if (pci_register_driver(&pci_driver) < 0) {
		printk(KERN_ERR "v2d: pci_register_driver\n");
		goto error_class;
	}

	return 0;

error_class:
	class_destroy(class);
error_chrdev:
	unregister_chrdev_region(devno, 1);
error_kmalloc:
	kfree(devices);
error:
	return -1;
}

static void
v2d_exit_module(void)
{
	int i;

	pci_unregister_driver(&pci_driver);
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


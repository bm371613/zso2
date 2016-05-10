#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include "pci_cdev.h"
#include "vintage2d.h"
#include "v2d_ioctl.h"
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
static struct pci_cdev *pci_cdev_table;

/* chardev interface */

static int
v2d_open(struct inode *inode, struct file *file)
{
	struct pci_dev *dev = pci_cdev_search_pci_dev(
		pci_cdev_table,
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

	if ((minor = pci_cdev_add(pci_cdev_table, max_devices, dev)) < 0) {
		dev_err(&(dev->dev), "pci_cdev_add\n");
		goto error;
	}

	cdev = cdev_alloc();
	cdev_init(cdev, &pci_ops);
	cdev->owner = THIS_MODULE;
	if (cdev_add(cdev, MKDEV(MAJOR(devno), minor), 1) != 0) {
		dev_err(&(dev->dev), "cdev_add\n");
		goto error_pci_cdev;
	}
	pci_cdev_table[minor].cdev = cdev;

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

	dev_info(&(dev->dev), "Registered device %d", minor);

	return 0;

error_pci_enable:
	pci_disable_device(dev);
error_cdev:
	cdev_del(cdev);
error_pci_cdev:
	pci_cdev_del(pci_cdev_table, max_devices, dev);
error:
	return -1;
}

static void
v2d_remove(struct pci_dev *dev)
{
	int minor;
	struct cdev *cdev;

	pci_release_regions(dev);
	pci_disable_device(dev);

	minor = pci_cdev_search_minor(pci_cdev_table, max_devices, dev);
	cdev = pci_cdev_search_cdev(pci_cdev_table, max_devices, minor);
	device_destroy(class, MKDEV(MAJOR(devno), minor));
	if (cdev != NULL)
		cdev_del(cdev);
	pci_cdev_del(pci_cdev_table, max_devices, dev);
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
	pci_cdev_table = kmalloc(max_devices * sizeof(struct pci_cdev),
			GFP_KERNEL);
	if (!pci_cdev_table) {
		printk(KERN_ERR "v2d: kmalloc\n");
		goto error;
	}

	if (alloc_chrdev_region(&devno, 0, max_devices, "v2d") < 0) {
		printk(KERN_ERR "v2d: alloc_chrdev_region\n");
		goto error_kmalloc;
	}

	pci_cdev_init(pci_cdev_table, max_devices, MINOR(devno));

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
	kfree(pci_cdev_table);
error:
	return -1;
}

static void
v2d_exit_module(void)
{
	int i;

	pci_unregister_driver(&pci_driver);
	for (i=0; i< max_devices; i++) {
		if (pci_cdev_table[i].pci_dev != NULL) {
			cdev_del(pci_cdev_table[i].cdev);
		}
	}
	class_destroy(class);
	unregister_chrdev_region(devno, max_devices);
	kfree(pci_cdev_table);
}

module_init(v2d_init_module);
module_exit(v2d_exit_module);

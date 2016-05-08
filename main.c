#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include "pci_cdev.h"

#define MAX_DEVICE	8

MODULE_LICENSE("GPL");

static struct pci_device_id v2d_ids[] = {
	{ PCI_DEVICE(0x1af4, 0x10f2), },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, v2d_ids);

static dev_t devno;
static struct class *class;
static struct pci_cdev pci_cdev[MAX_DEVICE];

/* chardev interace */

static int
v2d_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);
	file->private_data = (void *) pci_cdev_search_pci_dev(
		pci_cdev,
		MAX_DEVICE,
		minor);
	return 0;
}

static int
v2d_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t
v2d_read(struct file *file, char *buffer, size_t length, loff_t * offset)
{
	return 0;
}

static ssize_t
v2d_write(struct file *filp, const char *buffer, size_t len, loff_t * off) {
	return len;
}

static struct file_operations pci_ops = {
	.owner		= THIS_MODULE,
	.read 		= v2d_read,
	.write 		= v2d_write,
	.open 		= v2d_open,
	.release 	= v2d_release
};

/* pci interface */

static int
v2d_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int minor;
	struct cdev *cdev;
	struct device *device = NULL;

	if ((minor = pci_cdev_add(pci_cdev, MAX_DEVICE, dev)) < 0) {
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
	pci_cdev[minor].cdev = cdev;

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

	dev_info(&(dev->dev), "Registered device %d", minor);

	return 0;

error_pci_enable:
	pci_disable_device(dev);
error_cdev:
	cdev_del(cdev);
error_pci_cdev:
	pci_cdev_del(pci_cdev, MAX_DEVICE, dev);
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

	minor = pci_cdev_search_minor(pci_cdev, MAX_DEVICE, dev);
	cdev = pci_cdev_search_cdev(pci_cdev, MAX_DEVICE, minor);
	device_destroy(class, MKDEV(MAJOR(devno), minor));
	if (cdev != NULL)
		cdev_del(cdev);
	pci_cdev_del(pci_cdev, MAX_DEVICE, dev);
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
	if (alloc_chrdev_region(&devno, 0, MAX_DEVICE, "v2d") < 0) {
		printk(KERN_ERR "v2d: alloc_chrdev_region\n");
		goto error;
	}

	pci_cdev_init(pci_cdev, MAX_DEVICE, MINOR(devno));

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
error:
	return -1;
}

static void
v2d_exit_module(void)
{
	int i;

	pci_unregister_driver(&pci_driver);
	for (i=0; i< MAX_DEVICE; i++) {
		if (pci_cdev[i].pci_dev != NULL) {
			cdev_del(pci_cdev[i].cdev);
		}
	}
	unregister_chrdev_region(devno, MAX_DEVICE);
	class_destroy(class);
}

module_init(v2d_init_module);
module_exit(v2d_exit_module);


#ifndef V2D_DEVICE_H
#define V2D_DEVICE_H

#include <linux/pci.h>

typedef struct {
	int minor;
	struct pci_dev *dev;
	struct cdev *cdev;
	void __iomem *control;
} v2d_device_t;

void
v2d_devices_init(v2d_device_t v2d_devices[], int size, int first_minor);

v2d_device_t *
v2d_devices_add(v2d_device_t v2d_devices[], int size, struct pci_dev *dev);

void
v2d_devices_del(v2d_device_t v2d_devices[], int size, struct pci_dev *dev);

v2d_device_t *
v2d_devices_by_minor(v2d_device_t v2d_devices[], int size, int minor);

v2d_device_t *
v2d_devices_by_dev(v2d_device_t v2d_devices[], int size, struct pci_dev *dev);

#endif


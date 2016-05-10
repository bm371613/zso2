#include <linux/kernel.h>
#include "v2d_device.h"

void
v2d_devices_init(v2d_device_t v2d_devices[], int size, int first_minor)
{
	int i;

	for(i=0; i<size; i++) {
		v2d_devices[i].minor = first_minor++;
		v2d_devices[i].dev = NULL;
	}
}

v2d_device_t *
v2d_devices_add(v2d_device_t v2d_devices[], int size, struct pci_dev *dev)
{
	int i;

	for(i=0; i<size; i++) {
		if (v2d_devices[i].dev == NULL) {
			v2d_devices[i].dev = dev;
			return &v2d_devices[i];
		}
	}

	return NULL;
}

void
v2d_devices_del(v2d_device_t v2d_devices[], int size, struct pci_dev *dev)
{
	int i;

	for(i=0; i<size; i++) {
		if (v2d_devices[i].dev == dev) {
			v2d_devices[i].dev = NULL;
			return;
		}
	}
}

v2d_device_t *
v2d_devices_by_minor(v2d_device_t v2d_devices[], int size, int minor)
{
	int i;

	for(i=0; i<size; i++) {
		if (v2d_devices[i].minor == minor) {
			return &v2d_devices[i];
		}
	}
	return NULL;
}

v2d_device_t *
v2d_devices_by_dev(v2d_device_t v2d_devices[], int size, struct pci_dev *dev)
{
	int i;

	for(i=0; i<size; i++) {
		if (v2d_devices[i].dev == dev) {
			return &v2d_devices[i];
		}
	}
	return NULL;
}

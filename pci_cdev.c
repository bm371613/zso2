/* pci - cdev mapping (copied from pci-driver.c by tremyfr@yahoo.fr) */
#include <linux/kernel.h>
#include "pci_cdev.h"

void
pci_cdev_init(struct pci_cdev pci_cdev[], int size, int first_minor)
{
	int i;

	for(i=0; i<size; i++) {
		pci_cdev[i].minor   = first_minor++;
		pci_cdev[i].pci_dev = NULL;
		pci_cdev[i].cdev    = NULL;
	}
}

int
pci_cdev_add(struct pci_cdev pci_cdev[], int size, struct pci_dev *pdev)
{
	int i, res = -1;

	for(i=0; i<size; i++) {
		if (pci_cdev[i].pci_dev == NULL) {
			pci_cdev[i].pci_dev = pdev;
			res = pci_cdev[i].minor;
			break;
		}
	}

	return res;
}

void
pci_cdev_del(struct pci_cdev pci_cdev[], int size, struct pci_dev *pdev)
{
	int i;

	for(i=0; i<size; i++) {
		if (pci_cdev[i].pci_dev == pdev) {
			pci_cdev[i].pci_dev = NULL;
		}
	}
}

struct pci_dev *
pci_cdev_search_pci_dev(struct pci_cdev pci_cdev[], int size, int minor)
{
	int i;
	struct pci_dev *pdev = NULL;

	for(i=0; i<size; i++) {
		if (pci_cdev[i].minor == minor) {
			pdev = pci_cdev[i].pci_dev;
			break;
		}
	}

	return pdev;
}

struct cdev *
pci_cdev_search_cdev(struct pci_cdev pci_cdev[], int size, int minor)
{
	int i;
	struct cdev *cdev = NULL;

	for(i=0; i<size; i++) {
		if (pci_cdev[i].minor == minor) {
			cdev = pci_cdev[i].cdev;
			break;
		}
	}

	return cdev;
}

int
pci_cdev_search_minor(
		struct pci_cdev pci_cdev[],
		int size,
		struct pci_dev *pdev)
{
	int i, minor = -1;

	for(i=0; i<size; i++) {
		if (pci_cdev[i].pci_dev == pdev) {
			minor = pci_cdev[i].minor;
			break;
		}
	}

	return minor;
}

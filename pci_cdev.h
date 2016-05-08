struct pci_cdev {
	int minor;
	struct pci_dev *pci_dev;
	struct cdev *cdev;
};


void
pci_cdev_init(struct pci_cdev pci_cdev[], int size, int first_minor);

int
pci_cdev_add(struct pci_cdev pci_cdev[], int size, struct pci_dev *pdev);

void
pci_cdev_del(struct pci_cdev pci_cdev[], int size, struct pci_dev *pdev);

struct pci_dev *
pci_cdev_search_pci_dev(struct pci_cdev pci_cdev[], int size, int minor);

struct cdev *
pci_cdev_search_cdev(struct pci_cdev pci_cdev[], int size, int minor);

int
pci_cdev_search_minor(
		struct pci_cdev pci_cdev[],
		int size,
		struct pci_dev *pdev);


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <asm/uaccess.h>

#include "mydrm_api.h"

MODULE_LICENSE("GPL");

static struct pci_device_id pciidlist[] = {
	{0x1002, 0x68be, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
};

struct {
	unsigned int addr;
	unsigned int len;
	unsigned int flags;
} bar[7] = {{0}};

struct class *dev_class;
struct cdev mydrm_cdev;

static long mydrm_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int err;

	switch (cmd) {
	case MYDRM_GET_PCI_MMIO:
		if ((err = put_user(bar[2].addr, (uint32_t *)arg)) < 0)
			return err;
		if ((err = put_user(bar[2].len, (uint32_t *)arg + 1)) < 0)
			return err;
		break;
	case MYDRM_GET_PCI_LFB:
		if ((err = put_user(bar[0].addr, (uint32_t *)arg)) < 0)
			return err;
		if ((err = put_user(bar[0].len, (uint32_t *)arg + 1)) < 0)
			return err;
		break;
	case MYDRM_GET_PCI_ROM:
		if ((err = put_user(bar[6].addr, (uint32_t *)arg)) < 0)
			return err;
		if ((err = put_user(bar[6].len, (uint32_t *)arg + 1)) < 0)
			return err;
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

static int mydrm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
						vma->vm_end - vma->vm_start,
						vma->vm_page_prot))
		return -EAGAIN;
	return 0;
}

static const struct file_operations mydrm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mydrm_ioctl,
	.mmap = mydrm_mmap,
}; 

static int __devinit
mydrm_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int i;
	struct device *device; 

	printk(KERN_ERR "[MYDRM]: find card\n");

	for (i = 0; i < 6; i++) {
		bar[i].addr = pci_resource_start(pdev, i);
		bar[i].len = pci_resource_len(pdev, i);
		bar[i].flags = pci_resource_flags(pdev, i);
		printk(KERN_ERR "[MYDRM]: PCI BAR%d ADDR=%08x LEN=%x FLAGS=%x\n", 
			   i, bar[i].addr, bar[i].len, bar[i].flags);
	}

	bar[6].addr = pci_resource_start(pdev, PCI_ROM_RESOURCE);
	bar[6].len = pci_resource_len(pdev, PCI_ROM_RESOURCE);
	bar[6].flags = pci_resource_flags(pdev, PCI_ROM_RESOURCE);

	device = device_create(dev_class, NULL, MKDEV(MAJOR(mydrm_cdev.dev), 0), NULL, "mydrm");
	if (IS_ERR(device)) {
		printk(KERN_ERR "[MYDRM]: Create device fail!\n");
		return PTR_ERR(device);
	} 

	return 0;
}

static void
mydrm_pci_remove(struct pci_dev *pdev)
{
	device_destroy(dev_class, MKDEV(MAJOR(mydrm_cdev.dev), 0));
	printk(KERN_ERR "[MYDRM]: remove card\n");
}

static struct pci_driver mydrm_pci_driver = {
	.name = "mydrm",
	.id_table = pciidlist,
	.probe = mydrm_pci_probe,
	.remove = mydrm_pci_remove,
};

static int __init mydrm_init(void)
{
	int err;
	dev_t dev;

	dev_class = class_create(THIS_MODULE, "mydrm");
	if (IS_ERR(dev_class)) {
		printk(KERN_ERR "[MYDRM]: Create device class error\n");
		err = PTR_ERR(dev_class);
		goto err0;
	}

	if ((err = alloc_chrdev_region(&dev, 0, 1, "mydrm")) < 0) {
		printk(KERN_ERR "[MYDRM]: Alloc char device number fail!\n");
		goto err1;
	}
	cdev_init(&mydrm_cdev, &mydrm_fops);
	mydrm_cdev.owner = THIS_MODULE;
	if ((err = cdev_add(&mydrm_cdev, dev, 1)) < 0) {
		printk(KERN_ERR "[MYDRM]: Registering device fail!\n");
		goto err2;
	}

	if ((err = pci_register_driver(&mydrm_pci_driver)) < 0) {
		printk(KERN_ERR "Register PCI driver error\n");
		goto err3;
	}
	return 0;

 err3:
	cdev_del(&mydrm_cdev);
 err2:
	unregister_chrdev_region(dev, 1);
 err1:
	class_destroy(dev_class);
 err0:
	return err;
}

static void __exit mydrm_exit(void)
{
	dev_t dev = mydrm_cdev.dev;

	pci_unregister_driver(&mydrm_pci_driver);
	cdev_del(&mydrm_cdev);
	unregister_chrdev_region(dev, 1);
	class_destroy(dev_class);
}

module_init(mydrm_init);
module_exit(mydrm_exit);

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "avivod.h"
#include "evergreend.h"
#include "evergreen_reg.h"

static struct pci_device_id pciidlist[] = {
	{0x1002, 0x68be, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
};

struct {
	unsigned int addr;
	unsigned int len;
	unsigned int flags;
} bar[6] = {{0}};

void *mmiobase = 0;
unsigned int iobase = 0;
unsigned int *fb = 0;

static inline uint32_t RREG32(uint32_t reg)
{
	return readl(mmiobase + reg);
}

static inline void WREG32(uint32_t reg, uint32_t val)
{
	writel(val, mmiobase + reg);
}

static void __attribute__ ((unused)) card_reset(void)
{
	u32 grbm_reset = 0;

	// Disable CP parsing/prefetching
	WREG32(CP_ME_CNTL, CP_ME_HALT | CP_PFP_HALT);

	// reset all the gfx blocks
	grbm_reset = (SOFT_RESET_CP |
		      SOFT_RESET_CB |
		      SOFT_RESET_DB |
		      SOFT_RESET_PA |
		      SOFT_RESET_SC |
		      SOFT_RESET_SPI |
		      SOFT_RESET_SH |
		      SOFT_RESET_SX |
		      SOFT_RESET_TC |
		      SOFT_RESET_TA |
		      SOFT_RESET_VC |
		      SOFT_RESET_VGT);

	WREG32(GRBM_SOFT_RESET, grbm_reset);
	(void)RREG32(GRBM_SOFT_RESET);
	udelay(50);
	WREG32(GRBM_SOFT_RESET, 0);
	(void)RREG32(GRBM_SOFT_RESET);
	// Wait a little for things to settle down
	udelay(50);
}

static void print_primary_surface(void)
{
	uint32_t fbl = RREG32(MC_VM_FB_LOCATION);
	printk(KERN_ERR "[MYDRM]: INTERNAL ADDRESS SPACE FB LOCATION BASE=%08x TOP=%08x\n", 
		   fbl << 24, (fbl & 0xffff0000) << 8);

	printk(KERN_ERR "[MYDRM]: CONFIG_MEMSIZE=%x\n",
		   RREG32(CONFIG_MEMSIZE));

	printk(KERN_ERR "[MYDRM]: MC_ARB_RAMCFG=%x MC_SHARED_CHMAP=%x\n",
		   RREG32(MC_ARB_RAMCFG), RREG32(MC_SHARED_CHMAP));

	printk(KERN_ERR "[MYDRM]: MC_VM_SYSTEM_APERTURE_LOW_ADDR=%x MC_VM_SYSTEM_APERTURE_HIGH_ADDR=%x\n",
		   RREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR), RREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR));

	printk(KERN_ERR "[MYDRM]: VGA MEMORY BASE=%08x%08x\n",
		   RREG32(EVERGREEN_VGA_MEMORY_BASE_ADDRESS_HIGH),
		   RREG32(EVERGREEN_VGA_MEMORY_BASE_ADDRESS));

	printk(KERN_ERR "[MYDRM]: VGA HDP CONTROL=%x\n",
		   RREG32(VGA_HDP_CONTROL));

	printk(KERN_ERR "[MYDRM]: D1VGA_CONTROL=%x D2VGA_CONTROL=%x\n",
		   RREG32(D1VGA_CONTROL), RREG32(D2VGA_CONTROL));

	printk(KERN_ERR "[MYDRM]: VGA_RENDER_CONTROL=%x\n",
		   RREG32(VGA_RENDER_CONTROL));

	printk(KERN_ERR "[MYDRM]: CRTC_CONTROL CRTC0=%x CRTC1=%x\n",
		   RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET),
		   RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET));

	printk(KERN_ERR "[MYDRM]: CRTC_UPDATE_LOCK CRTC0=%x CRTC1=%x\n",
		   RREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC0_REGISTER_OFFSET),
		   RREG32(EVERGREEN_CRTC_UPDATE_LOCK + EVERGREEN_CRTC1_REGISTER_OFFSET));

	printk(KERN_ERR "[MYDRM]: CRTC FRAME COUNT=%d\n", 
		   RREG32(CRTC_STATUS_FRAME_COUNT + EVERGREEN_CRTC0_REGISTER_OFFSET));

	printk(KERN_ERR "[MYDRM]: CRTC0 PRIMARY SURFACE=%08x%08x SECONDARY SURFACE=%08x%08x\n", 
		   RREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC0_REGISTER_OFFSET),
		   RREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + EVERGREEN_CRTC0_REGISTER_OFFSET),
		   RREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC0_REGISTER_OFFSET),
		   RREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + EVERGREEN_CRTC0_REGISTER_OFFSET));

	printk(KERN_ERR "[MYDRM]: CRTC1 PRIMARY SURFACE=%08x%08x SECONDARY SURFACE=%08x%08x\n", 
		   RREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC1_REGISTER_OFFSET),
		   RREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC1_REGISTER_OFFSET),
		   RREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + EVERGREEN_CRTC1_REGISTER_OFFSET),
		   RREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + EVERGREEN_CRTC1_REGISTER_OFFSET));

	printk(KERN_ERR "[MYDRM]: GRPH_SURFACE_OFFSET_X=%x GRPH_SURFACE_OFFSET_Y=%x\n",
		   RREG32(EVERGREEN_GRPH_SURFACE_OFFSET_X + EVERGREEN_CRTC0_REGISTER_OFFSET),
		   RREG32(EVERGREEN_GRPH_SURFACE_OFFSET_Y + EVERGREEN_CRTC0_REGISTER_OFFSET));

	printk(KERN_ERR "[MYDRM]: GRPH_X_START=%x GRPH_Y_START=%x\n",
		   RREG32(EVERGREEN_GRPH_X_START + EVERGREEN_CRTC0_REGISTER_OFFSET),
		   RREG32(EVERGREEN_GRPH_Y_START + EVERGREEN_CRTC0_REGISTER_OFFSET));

	printk(KERN_ERR "[MYDRM]: GRPH_X_END=%x GRPH_Y_END=%x\n",
		   RREG32(EVERGREEN_GRPH_X_END + EVERGREEN_CRTC0_REGISTER_OFFSET),
		   RREG32(EVERGREEN_GRPH_Y_END + EVERGREEN_CRTC0_REGISTER_OFFSET));

	printk(KERN_ERR "[MYDRM]: VIEWPORT_START=%x VIEWPORT_SIZE=%x\n",
		   RREG32(EVERGREEN_VIEWPORT_START + EVERGREEN_CRTC0_REGISTER_OFFSET),
		   RREG32(EVERGREEN_VIEWPORT_SIZE + EVERGREEN_CRTC0_REGISTER_OFFSET));

	printk(KERN_ERR "[MYDRM]: GRPH_CONTROL=%x\n",
		   RREG32(EVERGREEN_GRPH_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET));

	printk(KERN_ERR "[MYDRM]: GB_ADDR_CONFIG=%x\n",
		   RREG32(GB_ADDR_CONFIG));
}

void draw(void)
{
	WREG32(EVERGREEN_VIEWPORT_START + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_VIEWPORT_SIZE + EVERGREEN_CRTC0_REGISTER_OFFSET, 0x4000300);
	memset(fb, 0xff, 0x10000);
}

static int __devinit
mydrm_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int i;

	printk(KERN_ERR "[MYDRM]: find card\n");

	for (i = 0; i < 6; i++) {
		bar[i].addr = pci_resource_start(pdev, i);
		bar[i].len = pci_resource_len(pdev, i);
		bar[i].flags = pci_resource_flags(pdev, i);
		printk(KERN_ERR "[MYDRM]: PCI BAR%d ADDR=%08x LEN=%x FLAGS=%x\n", 
			   i, bar[i].addr, bar[i].len, bar[i].flags);
	}

	mmiobase = ioremap(bar[2].addr, bar[2].len);
	if (mmiobase == NULL) {
		printk(KERN_ERR "[MYDRM]: MMIO remap fail\n");
		return -ENOMEM;
	}
	printk(KERN_ERR "[MYDRM]: MMIO remap to %08x\n", (unsigned int)mmiobase);

	iobase = bar[4].addr;

	//card_reset();

	print_primary_surface();

	fb = ioremap(bar[0].addr, 0x1000000);

	draw();

	return 0;
}

static void
mydrm_pci_remove(struct pci_dev *pdev)
{
	iounmap(fb);
	iounmap(mmiobase);
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

	if ((err = pci_register_driver(&mydrm_pci_driver)) < 0)
		return err;
	return 0;
}

static void __exit mydrm_exit(void)
{
	pci_unregister_driver(&mydrm_pci_driver);
}

module_init(mydrm_init);
module_exit(mydrm_exit);

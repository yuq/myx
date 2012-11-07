#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "mydrm.h"

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
// assume LFB MC base address = 0 and mapped to pci aperture address offset 0
unsigned int pci_aperture_offset = 0x0000000;
int fbw = 0x500, fbh = 0x500;
int monw = 0x500, monh = 0x400;

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

	printk(KERN_ERR "[MYDRM]: GRPH_CONTROL=%x GRPH_PITCH=%x\n",
		   RREG32(EVERGREEN_GRPH_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET),
		   RREG32(EVERGREEN_GRPH_PITCH + EVERGREEN_CRTC0_REGISTER_OFFSET));

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

	printk(KERN_ERR "[MYDRM]: HDP_HOST_PATH_CNTL=%x HDP_NONSURFACE_BASE=%x "
		   "HDP_NONSURFACE_INFO=%x HDP_NONSURFACE_SIZE=%x\n",
		   RREG32(HDP_HOST_PATH_CNTL), RREG32(HDP_NONSURFACE_BASE), 
		   RREG32(HDP_NONSURFACE_INFO), RREG32(HDP_NONSURFACE_SIZE));
}

void setcrtc(void)
{
	int i;
	uint32_t tmp = RREG32(EVERGREEN_GRPH_UPDATE + EVERGREEN_CRTC0_REGISTER_OFFSET);

	/* Lock the graphics update lock */
	tmp |= EVERGREEN_GRPH_UPDATE_LOCK;
	WREG32(EVERGREEN_GRPH_UPDATE + EVERGREEN_CRTC0_REGISTER_OFFSET, tmp);

	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + EVERGREEN_CRTC0_REGISTER_OFFSET, pci_aperture_offset);
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS_HIGH + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_GRPH_SECONDARY_SURFACE_ADDRESS + EVERGREEN_CRTC0_REGISTER_OFFSET, pci_aperture_offset);
	WREG32(EVERGREEN_GRPH_SURFACE_OFFSET_X + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_GRPH_SURFACE_OFFSET_Y + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	// this determine the surface width
	WREG32(EVERGREEN_GRPH_PITCH + EVERGREEN_CRTC0_REGISTER_OFFSET, fbw);
	// control which part of the surface will be displayed on monitor
	// 
	// Surface                  Monitor
	// +---------------+        +---------------+
	// | (x,y)         |        |       NA      |
	// |   +----+      |        |  +----+       |
	// |   |    |      |        |  |  A |       |
	// |   +----+      |        |  +----+       |
	// |     (xe,ye)   |        |               |
	// +---------------+        +---------------+
	WREG32(EVERGREEN_GRPH_X_START + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_GRPH_Y_START + EVERGREEN_CRTC0_REGISTER_OFFSET, 0);
	WREG32(EVERGREEN_GRPH_X_END + EVERGREEN_CRTC0_REGISTER_OFFSET, fbw);
	WREG32(EVERGREEN_GRPH_Y_END + EVERGREEN_CRTC0_REGISTER_OFFSET, fbh);
	// Surface                  Monitor
	// +---------------+        +---------------+
	// | (x,y)         |        |       NA      |
	// |   +----+      |        |     +----+    |
	// |   |    |      |        |     |  A |    |
	// |   +----+      |        |     +----+    |
	// |     (xe,ye)   |        |     center    |
	// +---------------+        +---------------+
	WREG32(EVERGREEN_VIEWPORT_START + EVERGREEN_CRTC0_REGISTER_OFFSET, 0x0000000);
	WREG32(EVERGREEN_VIEWPORT_SIZE + EVERGREEN_CRTC0_REGISTER_OFFSET, (monw << 16) | monh);
	WREG32(EVERGREEN_DESKTOP_HEIGHT + EVERGREEN_CRTC0_REGISTER_OFFSET, fbh);

	/* Wait for update_pending to go high. */
	for (i = 0; i < 1000; i++) {
		if (RREG32(EVERGREEN_GRPH_UPDATE + EVERGREEN_CRTC0_REGISTER_OFFSET) & EVERGREEN_GRPH_SURFACE_UPDATE_PENDING)
			break;
		udelay(1);
	}

	/* Unlock the lock, so double-buffering can take place inside vblank */
	tmp &= ~EVERGREEN_GRPH_UPDATE_LOCK;
	WREG32(EVERGREEN_GRPH_UPDATE + EVERGREEN_CRTC0_REGISTER_OFFSET, tmp);
}

void setup_mc(void)
{
	int i;

	WREG32(VGA_HDP_CONTROL, VGA_MEMORY_DISABLE);

	WREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR, 0 >> 12);
	WREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR, 0x1f000000 >> 12);

	WREG32(HDP_REG_COHERENCY_FLUSH_CNTL, 0);

	// HDP (Host Data Path) init
	for (i = 0; i < HDP_SURFACE_NUM; i++) {
		WREG32(HDP_SURFACE_LOWER_BOUND + i * HDP_SURFACE_OFFSET, 0);
		WREG32(HDP_SURFACE_UPPER_BOUND + i * HDP_SURFACE_OFFSET, 0);
		WREG32(HDP_SURFACE_BASE + i * HDP_SURFACE_OFFSET, 0);
		WREG32(HDP_SURFACE_INFO + i * HDP_SURFACE_OFFSET, 0);
		WREG32(HDP_SURFACE_SIZE + i * HDP_SURFACE_OFFSET, 0);
	}

	WREG32(HDP_NONSURFACE_BASE, (0 >> 8));
	WREG32(HDP_NONSURFACE_INFO, (2 << 7) | (1 << 30));
	WREG32(HDP_NONSURFACE_SIZE, 0x1f000000);
}

void draw(void)
{
	int i;
	/*
	int x=50, y=50, xe=100, ye=300;
	for (i = y; i < ye; i++)
		memset(fb + 4 * (fbw * i + x), 0xff, 4 * (xe - x));
	*/

	memset(fb + 4 * (fbw * 5 + 20), 0xff, 4 * fbw * 2);
	memset(fb + 4 * (fbw * 6 + 20), 0xff, 4 * fbw);
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

	setup_mc();

	setcrtc();

	fb = ioremap(bar[0].addr + pci_aperture_offset, 0x1000000);

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

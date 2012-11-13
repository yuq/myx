#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>

#include "mydrm.h"

MODULE_LICENSE("GPL");

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
unsigned char *atombios = 0;
// assume LFB MC base address = 0 and mapped to pci aperture address offset 0
unsigned int pci_aperture_offset = 0x0000000;
int fbw = 0x500, fbh = 0x400;
int monw = 0x500, monh = 0x400;

atom_context* gAtomContext = 0;

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
	evergreen_mc_program();
}

void draw(void)
{
	memset(fb + 4 * (fbw * 5 + 20), 0xff, 4 * fbw * 2);
	memset(fb + 4 * (fbw * 6 + 20), 0xff, 4 * fbw);
}

static bool atombios_map(struct pci_dev *pdev)
{
	uint8_t __iomem *bios;
	size_t size;

	/* XXX: some cards may return 0 for rom size? ddx has a workaround */
	bios = pci_map_rom(pdev, &size);
	if (!bios) {
		return false;
	}

	if (size == 0 || bios[0] != 0x55 || bios[1] != 0xaa) {
		pci_unmap_rom(pdev, bios);
		return false;
	}
	atombios = bios;
	return true;
}

uint32 _read32(uint32 offset)
{
	return RREG32(offset);
}


void _write32(uint32 offset, uint32 value)
{
	WREG32(offset, value);
}


// AtomBIOS cail register calls (are *4... no clue why)
uint32 Read32Cail(uint32 offset)
{
	return _read32(offset * 4);
}


void Write32Cail(uint32 offset, uint32 value)
{
	_write32(offset * 4, value);
}

void atombios_init_scratch(void)
{
	uint32_t bios_2_scratch, bios_6_scratch;

	bios_2_scratch = RREG32(R600_BIOS_2_SCRATCH);
	bios_6_scratch = RREG32(R600_BIOS_6_SCRATCH);

	/* let the bios control the backlight */
	bios_2_scratch &= ~ATOM_S2_VRI_BRIGHT_ENABLE;

	/* tell the bios not to handle mode switching */
	bios_6_scratch |= ATOM_S6_ACC_BLOCK_DISPLAY_SWITCH;

	WREG32(R600_BIOS_2_SCRATCH, bios_2_scratch);
	WREG32(R600_BIOS_6_SCRATCH, bios_6_scratch);
}

bool atombios_isposted(void)
{
	// aka, is primary graphics card that POST loaded

	uint32 reg;

	// evergreen or higher
	reg = RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC0_REGISTER_OFFSET)
		| RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC1_REGISTER_OFFSET)
		| RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC2_REGISTER_OFFSET)
		| RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC3_REGISTER_OFFSET)
		| RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC4_REGISTER_OFFSET)
		| RREG32(EVERGREEN_CRTC_CONTROL + EVERGREEN_CRTC5_REGISTER_OFFSET);
	if ((reg & EVERGREEN_CRTC_MASTER_EN) != 0)
		return true;

	// also check memory size incase crt controlers are disabled
	reg = RREG32(R600_CONFIG_MEMSIZE);

	if (reg)
		return true;

	return false;
}

int atombios_init(void *bios)
{
	struct card_info* atom_card_info = kmalloc(sizeof(struct card_info), GFP_KERNEL);

	if (!atom_card_info)
		return -1;

	atom_card_info->reg_read = Read32Cail;
	atom_card_info->reg_write = Write32Cail;

	// use MMIO instead of PCI I/O BAR
	atom_card_info->ioreg_read = Read32Cail;
	atom_card_info->ioreg_write = Write32Cail;

	atom_card_info->mc_read = _read32;
	atom_card_info->mc_write = _write32;
	atom_card_info->pll_read = _read32;
	atom_card_info->pll_write = _write32;

	// Point AtomBIOS parser to card bios and malloc gAtomContext
	gAtomContext = atom_parse(atom_card_info, bios);

	if (gAtomContext == NULL) {
		printk(KERN_ERR "%s: couldn't parse system AtomBIOS\n", __func__);
		return -1;
	}

	atombios_init_scratch();
	atom_allocate_fb_scratch(gAtomContext);

	// post card atombios if needed
	if (atombios_isposted() == false) {
		printk(KERN_ERR "%s: init AtomBIOS for this card as it is not not posted\n", __func__);
		atom_asic_init(gAtomContext);
	} else {
		printk(KERN_ERR "%s: AtomBIOS is already posted\n", __func__);
	}

	return 0;
}

int radeon_gpu_probe(void)
{
	uint8 tableMajor;
	uint8 tableMinor;
	uint16 tableOffset;

	union atomFirmwareInfo {
		ATOM_FIRMWARE_INFO info;
		ATOM_FIRMWARE_INFO_V1_2 info_12;
		ATOM_FIRMWARE_INFO_V1_3 info_13;
		ATOM_FIRMWARE_INFO_V1_4 info_14;
		ATOM_FIRMWARE_INFO_V2_1 info_21;
		ATOM_FIRMWARE_INFO_V2_2 info_22;
	};
	union atomFirmwareInfo* firmwareInfo;

	int index;

	index = GetIndexIntoMasterTable(DATA, FirmwareInfo);
	if (atom_parse_data_header(gAtomContext, index, NULL, &tableMajor, &tableMinor, &tableOffset)) {
		printk(KERN_ERR "%s: Couldn't parse data header\n", __func__);
		return -1;
	}

	printk(KERN_ERR "%s: table %d.%d\n", __func__, tableMajor, tableMinor);

	firmwareInfo = (union atomFirmwareInfo*)(gAtomContext->bios + tableOffset);

	return 0;
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

	if (!atombios_map(pdev)) {
		printk(KERN_ERR "[MYDRM]: can't map ROM\n");
		goto err0;
	}

	mmiobase = ioremap(bar[2].addr, bar[2].len);
	if (mmiobase == NULL) {
		printk(KERN_ERR "[MYDRM]: MMIO remap fail\n");
		goto err1;
	}
	printk(KERN_ERR "[MYDRM]: MMIO remap to %08x\n", (unsigned int)mmiobase);

	iobase = bar[4].addr;

	if (atombios_init(atombios)) {
		goto err2;
	}

	if (radeon_gpu_probe()) {
		goto err3;
	}

	//card_reset();

	print_primary_surface();

	//setup_mc();

	//setcrtc();

	//pci_aperture_offset = RREG32(EVERGREEN_GRPH_PRIMARY_SURFACE_ADDRESS + EVERGREEN_CRTC0_REGISTER_OFFSET);

	fb = ioremap(bar[0].addr + pci_aperture_offset, 0x1000000);

	//draw();

	return 0;

 err3:
	kfree(gAtomContext->card);
	kfree(gAtomContext);
 err2:
	iounmap(mmiobase);
 err1:
	pci_unmap_rom(pdev, atombios);
 err0:
	return -1;
}

static void
mydrm_pci_remove(struct pci_dev *pdev)
{
	iounmap(fb);
	kfree(gAtomContext->card);
	kfree(gAtomContext);
	iounmap(mmiobase);
	pci_unmap_rom(pdev, atombios);
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

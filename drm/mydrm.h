#ifndef _MYDRM_H
#define _MYDRM_H

#include <linux/kernel.h>

#include "avivod.h"
#include "evergreend.h"
#include "evergreen_reg.h"

extern void *mmiobase;

static inline uint32_t RREG32(uint32_t reg)
{
	return readl(mmiobase + reg);
}

static inline void WREG32(uint32_t reg, uint32_t val)
{
	writel(val, mmiobase + reg);
}

struct evergreen_mc_save {
	u32 vga_control[6];
	u32 vga_render_control;
	u32 vga_hdp_control;
	u32 crtc_control[6];
};

#endif



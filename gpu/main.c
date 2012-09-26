#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <radeon_drm.h>
#include <radeon_bo.h>
#include <radeon_bo_gem.h>
#include <radeon_cs.h>
#include <radeon_cs_gem.h>

#include "evergreen_shader.h"
#include "evergreen_state.h"
#include "evergreen_reg.h"

struct drm {
	int fd;
	// for display
	drmModeCrtcPtr curr_crtc;
	drmModeConnectorPtr curr_connector;
	drmModeEncoderPtr curr_encoder;
	// for GPU
	struct radeon_bo_manager *bufmgr;
	struct radeon_cs_manager *csm;
	struct radeon_cs *cs;
} drm = {0};

void drm_init(void)
{
	int i;

    assert(drmAvailable());

    int fd;
	assert((fd = drmOpen("radeon", NULL)) >= 0);

    drmModeResPtr dmrp;
	assert((dmrp = drmModeGetResources(fd)) != NULL);

    // find a CRTC and connector to display
    drmModeConnectorPtr connector = NULL;
    for (i = 0; i < dmrp->count_connectors; i++) {
		drmModeConnectorPtr tmp = drmModeGetConnector(fd, dmrp->connectors[i]);
		if (tmp->connection == DRM_MODE_CONNECTED && tmp->count_modes > 0) {
			connector = tmp;
			break;
		}
	}
    assert(connector != NULL);

    drmModeEncoderPtr encoder;
    assert((encoder = drmModeGetEncoder(fd, connector->encoder_id)) != NULL);

    drmModeCrtcPtr crtc;
    assert((crtc = drmModeGetCrtc(fd, encoder->crtc_id)) != NULL);

	struct radeon_bo_manager *bufmgr;
    assert((bufmgr = radeon_bo_manager_gem_ctor(fd)) != NULL);

	struct radeon_cs_manager *csm;
	assert((csm = radeon_cs_manager_gem_ctor(fd)) != NULL);

	struct radeon_cs *cs;
	assert((cs = radeon_cs_create(csm, RADEON_BUFFER_SIZE/4)) != NULL);

	struct drm_radeon_gem_info gem_info;
    assert(drmCommandWriteRead(fd, DRM_RADEON_GEM_INFO, &gem_info, sizeof(gem_info)) == 0);
	radeon_cs_set_limit(cs, RADEON_GEM_DOMAIN_GTT, gem_info.gart_size);
	radeon_cs_set_limit(cs, RADEON_GEM_DOMAIN_VRAM, gem_info.vram_size);

	drm.fd = fd;
	drm.curr_crtc = crtc;
	drm.curr_connector = connector;
	drm.curr_encoder = encoder;

	drm.bufmgr = bufmgr;
	drm.csm = csm;
	drm.cs = cs;
}

struct radeon_bo *create_buffer(int width, int height)
{
    struct radeon_bo *bo;
	int depth = 24, bpp = 32;
	int pitch = width * (bpp / 8);
	int size = pitch * height;

    assert((bo = radeon_bo_open(drm.bufmgr, 0, size, 0x200, RADEON_GEM_DOMAIN_VRAM, 0)) != NULL);

    assert(radeon_bo_map(bo, 1) == 0);
    memset(bo->ptr, 0, size);

	return bo;
}

void gpu_draw(struct radeon_bo *dst_bo, int width, int height, int size, unsigned int fg)
{
	int shader_bo_size = 512 * 9;
	struct radeon_bo *shader_bo;
	assert((shader_bo = radeon_bo_open(drm.bufmgr, 0, shader_bo_size, 0, RADEON_GEM_DOMAIN_VRAM, 0)) != NULL);
	assert(radeon_bo_map(shader_bo, 1) == 0);
    memset(shader_bo->ptr, 0, shader_bo_size);

	uint32_t *shader = shader_bo->ptr;
	int solid_vs_offset = 0;
	evergreen_solid_vs(shader + solid_vs_offset / 4);
	int solid_ps_offset = 512;
	evergreen_solid_ps(shader + solid_ps_offset / 4);

	assert(radeon_bo_unmap(shader_bo) == 0);

	// from EVERGREENPrepareSolid()

	// from   R600SetAccelState()
	radeon_cs_space_reset_bos(drm.cs);
	radeon_cs_space_add_persistent_bo(drm.cs, shader_bo, RADEON_GEM_DOMAIN_VRAM, 0);
	radeon_cs_space_add_persistent_bo(drm.cs, dst_bo, 0, RADEON_GEM_DOMAIN_VRAM);
	assert(radeon_cs_space_check(drm.cs) == 0);

	evergreen_set_default_state(shader_bo);

	evergreen_set_generic_scissor(0, 0, width, height);
    evergreen_set_screen_scissor(0, 0, width, height);
    evergreen_set_window_scissor(0, 0, width, height);

	cb_config_t cb_conf;
    shader_config_t vs_conf, ps_conf;
	const_config_t ps_const_conf;
	CLEAR (cb_conf);
    CLEAR (vs_conf);
    CLEAR (ps_conf);
    CLEAR (ps_const_conf);

	vs_conf.shader_addr         = solid_vs_offset;
    vs_conf.shader_size         = 512;
    vs_conf.num_gprs            = 2;
    vs_conf.stack_size          = 0;
    vs_conf.bo                  = shader_bo;
    evergreen_vs_setup(&vs_conf, RADEON_GEM_DOMAIN_VRAM);

	ps_conf.shader_addr         = solid_ps_offset;
    ps_conf.shader_size         = 512;
    ps_conf.num_gprs            = 1;
    ps_conf.stack_size          = 0;
    ps_conf.clamp_consts        = 0;
    ps_conf.export_mode         = 2;
    ps_conf.bo                  = shader_bo;
    evergreen_ps_setup(&ps_conf, RADEON_GEM_DOMAIN_VRAM);


	int planemask = 0xffffffff;
	int rop = 3;
	int tiling_flags = 2;
	cb_conf.id = 0;
    cb_conf.w = width;
    cb_conf.h = height;
    cb_conf.base = 0;
    cb_conf.bo = dst_bo;
	cb_conf.format = COLOR_8_8_8_8;
	cb_conf.comp_swap = 1; /* ARGB */
    cb_conf.source_format = EXPORT_4C_16BPC;
    cb_conf.blend_clamp = 1;
    /* Render setup */
    if (planemask & 0x000000ff)
		cb_conf.pmask |= 4; /* B */
    if (planemask & 0x0000ff00)
		cb_conf.pmask |= 2; /* G */
    if (planemask & 0x00ff0000)
		cb_conf.pmask |= 1; /* R */
    if (planemask & 0xff000000)
		cb_conf.pmask |= 8; /* A */
    cb_conf.rop = rop;
    if (tiling_flags == 0) {
		cb_conf.array_mode = 1;
		cb_conf.non_disp_tiling = 1;
    }
    evergreen_set_render_target(&cb_conf, RADEON_GEM_DOMAIN_VRAM);

	evergreen_set_spi(0, 0);

	struct radeon_bo *cbuf_bo;
	int cbuf_offset = 0;
	assert((cbuf_bo = radeon_bo_open(drm.bufmgr, 0, (16*1024), 0, RADEON_GEM_DOMAIN_GTT, 0)) != NULL);
	radeon_bo_ref(cbuf_bo);
	assert(radeon_bo_map(cbuf_bo, 1) == 0);
	assert(radeon_cs_space_check_with_bo(drm.cs, cbuf_bo, RADEON_GEM_DOMAIN_GTT, 0) == 0);

	/* PS alu constants */
	float *ps_alu_consts;
    ps_const_conf.size_bytes = 256;
    ps_const_conf.type = SHADER_TYPE_PS;
    ps_alu_consts = cbuf_bo->ptr + cbuf_offset;
    ps_const_conf.bo = cbuf_bo;
    ps_const_conf.const_addr = 0;
    ps_const_conf.cpu_ptr = (uint32_t *)(char *)ps_alu_consts;

	uint32_t a, r, g, b;
	a = (fg >> 24) & 0xff;
	r = (fg >> 16) & 0xff;
	g = (fg >> 8) & 0xff;
	b = (fg >> 0) & 0xff;
	ps_alu_consts[0] = (float)r / 255; /* R */
	ps_alu_consts[1] = (float)g / 255; /* G */
	ps_alu_consts[2] = (float)b / 255; /* B */
	ps_alu_consts[3] = (float)a / 255; /* A */

    cbuf_offset += 1 * 256;
    evergreen_set_alu_consts(&ps_const_conf, RADEON_GEM_DOMAIN_GTT);

	struct radeon_bo *vbo_bo;
	int vbo_offset = 0;
	assert((vbo_bo = radeon_bo_open(drm.bufmgr, 0, (16*1024), 0, RADEON_GEM_DOMAIN_GTT, 0)) != NULL);
	radeon_bo_ref(vbo_bo);
	assert(radeon_bo_map(vbo_bo, 1) == 0);
	assert(radeon_cs_space_check_with_bo(drm.cs, vbo_bo, RADEON_GEM_DOMAIN_GTT, 0) == 0);

	// from EVERGREENSolid()
	int x1 = 100;
	int y1 = 100;
	int x2 = 400;
	int y2 = 400;
	float *vb = vbo_bo->ptr + vbo_offset;
	vb[0] = (float)x1;
    vb[1] = (float)y1;

    vb[2] = (float)x1;
    vb[3] = (float)y2;

    vb[4] = (float)x2;
    vb[5] = (float)y2;
	vbo_offset += 3 * 8;

	// EVERGREENDoneSolid()
	evergreen_finish_op(dst_bo, size, vbo_bo, vbo_offset, 8);

	// radeon_cs_flush_indirect()
	radeon_bo_unmap(vbo_bo);
	radeon_bo_unref(vbo_bo);
	radeon_bo_unmap(cbuf_bo);
	radeon_bo_unref(cbuf_bo);
	radeon_cs_emit(drm.cs);
}

int main()
{
    drm_init();

	int width = drm.curr_crtc->mode.hdisplay;
	int height = drm.curr_crtc->mode.vdisplay;
	int depth = 24, bpp = 32;
	int pitch = width * (bpp / 8);
	int size = pitch * height;
	
    struct radeon_bo *bo = create_buffer(width, height);

	// CPU draw
	memset(bo->ptr, 0xff, pitch * 10);

	// GPU draw
	gpu_draw(bo, width, height, size, 0xffff0000);

	int fb_id;
    assert(drmModeAddFB(drm.fd, width, height, depth, bpp, pitch, bo->handle, &fb_id) == 0);

	assert(drmModeSetCrtc(drm.fd, drm.curr_crtc->crtc_id, fb_id, 0, 0, 
						  &drm.curr_connector->connector_id, 1, &drm.curr_crtc->mode) == 0);

	sleep(5);

	// restore previous framebuffer
	assert(drmModeSetCrtc(drm.fd, drm.curr_crtc->crtc_id, drm.curr_crtc->buffer_id, 
						  drm.curr_crtc->x, drm.curr_crtc->y, &drm.curr_connector->connector_id, 
						  1, &drm.curr_crtc->mode) == 0);

    drmClose(drm.fd);

    return 0;
}

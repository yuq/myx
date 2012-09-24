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

void gpu_draw(struct radeon_bo *dst_bo, int width, int height)
{
	int size = 512 * 9;

	struct radeon_bo *shader_bo;
	assert((shader_bo = radeon_bo_open(drm.bufmgr, 0, size, 0, RADEON_GEM_DOMAIN_VRAM, 0)) != NULL);
	assert(radeon_bo_map(shader_bo, 1) == 0);
    memset(shader_bo->ptr, 0, size);

	uint32_t *shader = shader_bo->ptr;
	int solid_vs_offset = 0;
	evergreen_solid_vs(shader + solid_vs_offset / 4);
	int solid_ps_offset = 512;
	evergreen_solid_ps(shader + solid_ps_offset / 4);

	assert(radeon_bo_unmap(shader_bo) == 0);

	// from EVERGREENPrepareSolid()
	// from   R600SetAccelState()
	radeon_cs_space_reset_bos(drm.cs);
	radeon_cs_space_add_persistent_bo(drm.cs, shaders_bo, RADEON_GEM_DOMAIN_VRAM, 0);
	radeon_cs_space_add_persistent_bo(drm.cs, dst_bo, 0, RADEON_GEM_DOMAIN_VRAM);
	assert(radeon_cs_space_check(drm.cs) == 0);

	evergreen_set_default_state(shader_bo);

	evergreen_set_generic_scissor(0, 0, width, height);
    evergreen_set_screen_scissor(0, 0, width, height);
    evergreen_set_window_scissor(0, 0, width, height);

	vs_conf.shader_addr         = solid_vs_offset;
    vs_conf.shader_size         = 512;
    vs_conf.num_gprs            = 2;
    vs_conf.stack_size          = 0;
    vs_conf.bo                  = shaders_bo;
    evergreen_vs_setup(&vs_conf, RADEON_GEM_DOMAIN_VRAM);
}

int main()
{
    drm_init();

	int width = drm.curr_crtc->mode.hdisplay;
	int height = drm.curr_crtc->mode.vdisplay;
	int depth = 24, bpp = 32;
	int pitch = width * (bpp / 8);
    struct radeon_bo *bo = create_buffer(width, height);

	// CPU draw
	memset(bo->ptr, 0xff, pitch * 10);

	// GPU draw
	gpu_draw(bo, width, height);

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

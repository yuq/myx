#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <radeon_drm.h>
#include <radeon_bo.h>
#include <radeon_bo_gem.h>

int main()
{
    int err;

    printf("DRM AVA: %d\n", drmAvailable());
    int fd;
    drmVersionPtr verp;
    if ((fd = drmOpen("radeon", NULL)) < 0) {
	fprintf(stderr, "drmOpen error %d\n", fd);
	return -1;
    }
    if ((verp = drmGetVersion(fd)) == NULL) {
	fprintf(stderr, "drmGetVersion error\n");
	return -1;
    }
    printf("DRM VER: major %d  minor %d  patch %d  name %s  date %s  desc %s\n", 
	   verp->version_major, 
	   verp->version_minor, 
	   verp->version_patchlevel,  
	   verp->name, 
	   verp->date, 
	   verp->desc);
    drmFreeVersion(verp);
    if ((verp = drmGetLibVersion(fd)) == NULL) {
	fprintf(stderr, "drmGetLibVersion error\n");
	return -1;
    }
    printf("DRM LIB VER: major %d  minor %d  patch %d\n", 
	   verp->version_major, 
	   verp->version_minor, 
	   verp->version_patchlevel);
    drmFreeVersion(verp);

    char *busid = drmGetBusid(fd);
    if (busid == NULL) {
	fprintf(stderr, "drmGetBusid error\n");
	return -1;
    }
    printf("DRM BUSID: %s\n", busid);

    drmModeResPtr dmrp = drmModeGetResources(fd);
    if (dmrp == NULL) {
	fprintf(stderr, "drmModeGetResources error\n");
	return -1;
    }
    printf("DRM MODE RES: nfb=%d ncrtc=%d nconn=%d nenc=%d minw=%d maxw=%d minh=%d maxh=%d\n", 
	   dmrp->count_fbs,
	   dmrp->count_crtcs,
	   dmrp->count_connectors,
	   dmrp->count_encoders,
	   dmrp->min_width,
	   dmrp->max_width,
	   dmrp->min_height,
	   dmrp->max_height);

    int i;
    drmModeFBPtr *dmfp;
    assert((dmfp = malloc(sizeof(drmModeFBPtr) * dmrp->count_fbs)) != NULL);
    for (i = 0; i < dmrp->count_fbs; i++) {
	assert((dmfp[i] = drmModeGetFB(fd, dmrp->fbs[i])) != NULL);
	printf("DRM FB: id=%d width=%d height=%d pitch=%d bpp=&d depth=%d handle=%d\n",
	       dmfp[i]->fb_id,
	       dmfp[i]->width,
	       dmfp[i]->height,
	       dmfp[i]->pitch,
	       dmfp[i]->bpp,
	       dmfp[i]->depth,
	       dmfp[i]->handle);
    }

    drmModeCrtcPtr *dmcp;
    assert((dmcp = malloc(sizeof(drmModeCrtcPtr) * dmrp->count_crtcs)) != NULL);
    for (i = 0; i < dmrp->count_crtcs; i++) {
	assert((dmcp[i] = drmModeGetCrtc(fd, dmrp->crtcs[i])) != NULL);
	printf("DRM CRTC: id=%d bufid=%d x=%d y=%d w=%d h=%d\n", 
	       dmcp[i]->crtc_id, 
	       dmcp[i]->buffer_id, 
	       dmcp[i]->x, dmcp[i]->y,
	       dmcp[i]->width, dmcp[i]->height);
    }

    drmModeConnectorPtr *dmnp;
    assert((dmnp = malloc(sizeof(drmModeConnectorPtr) * dmrp->count_connectors)) != NULL);
    for (i = 0; i < dmrp->count_connectors; i++) {
	assert((dmnp[i] = drmModeGetConnector(fd, dmrp->connectors[i])) != NULL);
	char *status;
	switch(dmnp[i]->connection) {
	case DRM_MODE_CONNECTED:
	    status = "connected";
	    break;
	case DRM_MODE_DISCONNECTED:
	    status = "disconnected";
	    break;
	default:
	case DRM_MODE_UNKNOWNCONNECTION:
	    status = "unknown";
	    break;
	}
	printf("DRM connector: con_id=%d enc_id=%d n_mode=%d n_prop=%d n_enc=%d conn=%s\n", 
	       dmnp[i]->connector_id, 
	       dmnp[i]->encoder_id, 
	       dmnp[i]->count_modes,
	       dmnp[i]->count_props,
	       dmnp[i]->count_encoders,
	       status);
    }

    drmModeEncoderPtr *dmep;
    assert((dmep = malloc(sizeof(drmModeEncoderPtr) * dmrp->count_encoders)) != NULL);
    for (i = 0; i < dmrp->count_encoders; i++) {
	assert((dmep[i] = drmModeGetEncoder(fd, dmrp->encoders[i])) != NULL);
	printf("DRM encoder: enc_id=%d crtc_id=%d poss_crtc=%d poss_clone=%d\n", 
	       dmep[i]->encoder_id, 
	       dmep[i]->crtc_id, 
	       dmep[i]->possible_crtcs,
	       dmep[i]->possible_clones);
    }

    struct drm_radeon_gem_info gem_info;
    if ((err = drmCommandWriteRead(fd, DRM_RADEON_GEM_INFO, &gem_info, sizeof(gem_info))) < 0) {
	fprintf(stderr, "drmCommandWriteRead DRM_RADEON_GEM_INFO error %d\n", err);
	return -1;
    }
    printf("DRM GEM INFO: gart_size=%llx vram_size=%llx vram_visible=%llx\n", 
	   gem_info.gart_size, 
	   gem_info.vram_size,
	   gem_info.vram_visible);

    struct radeon_bo_manager *bufmgr;
    assert((bufmgr = radeon_bo_manager_gem_ctor(fd)) != NULL);
    struct radeon_bo *bo;
    assert((bo = radeon_bo_open(bufmgr, 0, 0x500000, 0x200, RADEON_GEM_DOMAIN_VRAM, 0)) != NULL);
    assert(radeon_bo_map(bo, 1) == 0);
    printf("map bo %x\n", (uint32_t)bo->ptr);
    memset(bo->ptr, 0xff, 0x200000);

    int fb_id;
    assert(drmModeAddFB(fd, 1280, 1024, 24, 32, 5120, bo->handle, &fb_id) == 0);

    int output = 17;
    drmModeModeInfo mode;
    mode.clock = 108000;
    mode.hdisplay = 1280;
    mode.hsync_start = 1328;
    mode.hsync_end = 1440;
    mode.htotal = 1688;
    mode.hskew = 0;
    mode.vdisplay = 1024;
    mode.vsync_start = 1025;
    mode.vsync_end = 1028;
    mode.vtotal = 1066;
    mode.vscan = 0;
    mode.vrefresh = 0;
    mode.flags = 5;
    mode.type = 0;
    mode.name[0] = '\0';
    assert(drmModeSetCrtc(fd, 10, fb_id, 0, 0, &output, 1, &mode) == 0);

    sleep(15);
    
    drmClose(fd);
    return 0;
}

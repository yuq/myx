#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <radeon_drm.h>
#include <radeon_bo.h>
#include <radeon_bo_gem.h>

#include <cairo.h>

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

    // find a CRTC and connector to display
    drmModeConnectorPtr connector = NULL;
    for (i = 0; i < dmrp->count_connectors; i++)
		if (dmnp[i]->connection == DRM_MODE_CONNECTED && dmnp[i]->count_modes > 0) {
			connector = dmnp[i];
			break;
		}
    assert(connector != NULL);

    drmModeEncoderPtr encoder;
    assert((encoder = drmModeGetEncoder (fd, connector->encoder_id)) != NULL);

    drmModeCrtcPtr crtc;
    assert((crtc = drmModeGetCrtc (fd, encoder->crtc_id)) != NULL);

    drmModeModeInfoPtr mode = &crtc->mode;
    printf("Dislay CRTC mode: %s vd=%d hd=%d\n", mode->name, mode->vdisplay, mode->hdisplay);

    drmModeFBPtr fb;
    assert((fb = drmModeGetFB(fd, crtc->buffer_id)) != NULL);
    printf("Display CRTC FB: id=%d width=%d height=%d pitch=%d bpp=%d depth=%d\n",
		   fb->fb_id,
		   fb->width,
		   fb->height,
		   fb->pitch,
		   fb->bpp,
		   fb->depth);
    
    struct radeon_bo_manager *bufmgr;
    assert((bufmgr = radeon_bo_manager_gem_ctor(fd)) != NULL);
    struct radeon_bo *bo;
	int width = 1280, height = 1024, depth = 24, bpp = 32;
	int pitch = width * (bpp / 8);
	int screen_size = pitch * height;
    assert((bo = radeon_bo_open(bufmgr, 0, screen_size, 0x200, RADEON_GEM_DOMAIN_VRAM, 0)) != NULL);
    assert(radeon_bo_map(bo, 1) == 0);
    printf("map bo %x\n", (uint32_t)bo->ptr);
    memset(bo->ptr, 0, screen_size);

	cairo_surface_t *surface;
	surface = cairo_image_surface_create_for_data(bo->ptr,
												  CAIRO_FORMAT_ARGB32,
												  width,
												  height,
												  pitch);
	cairo_t *cr;
	cr = cairo_create(surface);

	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_rectangle (cr, 0, 0, 500, 500);
	cairo_fill (cr);

	double xc = 128.0;
	double yc = 128.0;
	double radius = 100.0;
	double angle1 = 45.0  * (M_PI/180.0);  /* angles are specified */
	double angle2 = 180.0 * (M_PI/180.0);  /* in radians           */

	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_set_line_width (cr, 10.0);
	cairo_arc (cr, xc, yc, radius, angle1, angle2);
	cairo_stroke (cr);

	/* draw helping lines */
	cairo_set_source_rgba (cr, 1, 0.2, 0.2, 0.6);
	cairo_set_line_width (cr, 6.0);

	cairo_arc (cr, xc, yc, 10.0, 0, 2*M_PI);
	cairo_fill (cr);

	cairo_arc (cr, xc, yc, radius, angle1, angle1);
	cairo_line_to (cr, xc, yc);
	cairo_arc (cr, xc, yc, radius, angle2, angle2);
	cairo_line_to (cr, xc, yc);
	cairo_stroke (cr);

    int fb_id;
    assert(drmModeAddFB(fd, width, height, depth, bpp, pitch, bo->handle, &fb_id) == 0);

    assert(drmModeSetCrtc(fd, crtc->crtc_id, fb_id, 0, 0, &connector->connector_id, 1, &crtc->mode) == 0);
    sleep(10);
    assert(drmModeSetCrtc(fd, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &connector->connector_id, 1, &crtc->mode) == 0);
    
    drmClose(fd);
    return 0;
}

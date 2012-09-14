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

#include <cairo.h>

struct drm {
	int fd;
	drmModeResPtr res;
	drmModeCrtcPtr *crtcs;
	drmModeConnectorPtr *connectors;
	drmModeEncoderPtr *encoders;
	drmModeCrtcPtr curr_crtc;
	drmModeConnectorPtr curr_connector;
	drmModeEncoderPtr curr_encoder;
} drm = {0};

#define FB_NUM 2
#define FRAME_RATE 20

struct fb_fifo {
	int fb_ids[FB_NUM];
	int fb_val[FB_NUM];
	int rp;
	int wp;
	pthread_mutex_t wmutex;
	pthread_mutex_t rmutex;
	pthread_cond_t wcond;
	pthread_cond_t rcond;
} fb_fifo = {0};

void drm_init(void)
{
    assert(drmAvailable());

    int fd;
	assert((fd = drmOpen("radeon", NULL)) >= 0);

    drmVersionPtr verp;
    assert((verp = drmGetVersion(fd)) != NULL);
    printf("DRM VER: major %d  minor %d  patch %d  name %s  date %s  desc %s\n", 
		   verp->version_major, 
		   verp->version_minor, 
		   verp->version_patchlevel,  
		   verp->name, 
		   verp->date, 
		   verp->desc);
    drmFreeVersion(verp);

    assert((verp = drmGetLibVersion(fd)) != NULL);
    printf("DRM LIB VER: major %d  minor %d  patch %d\n", 
		   verp->version_major, 
		   verp->version_minor, 
		   verp->version_patchlevel);
    drmFreeVersion(verp);

    char *busid;
	assert((busid = drmGetBusid(fd)) != NULL);
    printf("DRM BUSID: %s\n", busid);

    drmModeResPtr dmrp;
	assert((dmrp = drmModeGetResources(fd)) != NULL);
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
    assert(drmCommandWriteRead(fd, DRM_RADEON_GEM_INFO, &gem_info, sizeof(gem_info)) == 0);
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

	drm.fd = fd;
	drm.res = dmrp;
	drm.crtcs = dmcp;
	drm.connectors = dmnp;
	drm.encoders = dmep;
	drm.curr_crtc = crtc;
	drm.curr_connector = connector;
	drm.curr_encoder = encoder;
}

cairo_t *create_framebuffer(void)
{
	static int i = 0;
	assert(i < FB_NUM);

	struct radeon_bo_manager *bufmgr;
    assert((bufmgr = radeon_bo_manager_gem_ctor(drm.fd)) != NULL);

    struct radeon_bo *bo;
	int width = drm.curr_crtc->mode.hdisplay, height = drm.curr_crtc->mode.vdisplay;
	int depth = 24, bpp = 32;
	int pitch = width * (bpp / 8);
	int screen_size = pitch * height;
    assert((bo = radeon_bo_open(bufmgr, 0, screen_size, 0x200, RADEON_GEM_DOMAIN_VRAM, 0)) != NULL);

    assert(radeon_bo_map(bo, 1) == 0);
    printf("map bo %x\n", (uint32_t)bo->ptr);
    memset(bo->ptr, 0, screen_size);

	int fb_id;
    assert(drmModeAddFB(drm.fd, width, height, depth, bpp, pitch, bo->handle, &fb_id) == 0);
	fb_fifo.fb_ids[i++] = fb_id;

	cairo_surface_t *surface;
	surface = cairo_image_surface_create_for_data(bo->ptr,
												  CAIRO_FORMAT_ARGB32,
												  width,
												  height,
												  pitch);

	cairo_t *cr;
	cr = cairo_create(surface);

	return cr;
}

void draw_frame(cairo_t *cr, double ma, double ha)
{
	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
	cairo_rectangle (cr, 0, 0, 300, 300);
	cairo_fill (cr);

	double xc = 150.0;
	double yc = 150.0;
	double dial_radius = 110.0;
	double hour_radius = 80.0;
	double minute_radius = 100.0;

	// draw dial
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_set_line_width (cr, 8.0);
	cairo_arc (cr, xc, yc, dial_radius, 0, 2 * M_PI);
	cairo_stroke (cr);

	// draw hour hand
	cairo_set_source_rgba (cr, 1, 0.2, 0.2, 0.6);
	cairo_set_line_width (cr, 6.0);
	cairo_arc (cr, xc, yc, hour_radius, 0, ha);
	cairo_line_to (cr, xc, yc);
	cairo_stroke (cr);

	// draw minute hand
	cairo_set_source_rgba (cr, 1, 0.2, 0.6, 0.2);
	cairo_set_line_width (cr, 4.0);
	cairo_arc (cr, xc, yc, minute_radius, 0, ma);
	cairo_line_to (cr, xc, yc);
	cairo_stroke (cr);
}

static void user_abort(int dummy)
{
	assert(drmModeSetCrtc(drm.fd, drm.curr_crtc->crtc_id, drm.curr_crtc->buffer_id, 
						  drm.curr_crtc->x, drm.curr_crtc->y, &drm.curr_connector->connector_id, 
						  1, &drm.curr_crtc->mode) == 0);
    
    drmClose(drm.fd);
    exit(0);
}

static void *frame_update(void *arg)
{
	assert(pthread_mutex_lock(&fb_fifo.rmutex) == 0);
	while (fb_fifo.fb_val[fb_fifo.rp] != 1)
		assert(pthread_cond_wait(&fb_fifo.rcond, &fb_fifo.rmutex) == 0);
	assert(pthread_mutex_unlock(&fb_fifo.rmutex) == 0);

	drmEventContext evctx;
	evctx.version = DRM_EVENT_CONTEXT_VERSION;
	evctx.vblank_handler = NULL;
	evctx.page_flip_handler = NULL;

	struct timespec ts;
	assert(clock_gettime (CLOCK_MONOTONIC, &ts) == 0);
	uint64_t t = ts.tv_sec;
	uint32_t delta = 1000000000 / FRAME_RATE;
	t *= 1000000000;
	t += ts.tv_nsec;

	int old_rp = -1;
	while (1) {
		/*
		assert(drmModeSetCrtc(drm.fd, drm.curr_crtc->crtc_id, fb_fifo.fb_ids[fb_fifo.rp], 0, 0, 
							  &drm.curr_connector->connector_id, 1, &drm.curr_crtc->mode) == 0);
		//*/
		assert(drmModePageFlip(drm.fd, drm.curr_crtc->crtc_id, fb_fifo.fb_ids[fb_fifo.rp], DRM_MODE_PAGE_FLIP_EVENT, 0) == 0);

		// wait for page flip complete
		assert(drmHandleEvent(drm.fd, &evctx) == 0);

		if (old_rp >= 0) {
			assert(pthread_mutex_lock(&fb_fifo.wmutex) == 0);
			fb_fifo.fb_val[old_rp] = 0;
			assert(pthread_mutex_unlock(&fb_fifo.wmutex) == 0);
			assert(pthread_cond_signal(&fb_fifo.wcond) == 0);
		}

		old_rp = fb_fifo.rp++;
		if (fb_fifo.rp >= FB_NUM)
			fb_fifo.rp = 0;

		assert(pthread_mutex_lock(&fb_fifo.rmutex) == 0);
		while (fb_fifo.fb_val[fb_fifo.rp] != 1)
			assert(pthread_cond_wait(&fb_fifo.rcond, &fb_fifo.rmutex) == 0);
		assert(pthread_mutex_unlock(&fb_fifo.rmutex) == 0);

		t += delta;
		ts.tv_sec = t / 1000000000;
		ts.tv_nsec = t % 1000000000;
		assert(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == 0);
	}
}

int main()
{
    drm_init();

	int i;
	cairo_t *cr[FB_NUM];
	for (i = 0; i < FB_NUM; i++)
		cr[i] = create_framebuffer();

	signal(SIGINT, user_abort);

	pthread_attr_t attr;
	pthread_t tid;
	assert(pthread_mutex_init(&fb_fifo.wmutex, NULL) == 0);
	assert(pthread_mutex_init(&fb_fifo.rmutex, NULL) == 0);
	assert(pthread_cond_init(&fb_fifo.wcond, NULL) == 0);
	assert(pthread_cond_init(&fb_fifo.rcond, NULL) == 0);
	assert(pthread_attr_init(&attr) == 0);
	assert(pthread_create(&tid, &attr, frame_update, NULL) == 0);

	double ma = 0, ha = 0;
	double periodic = 10;
	while (1) {
		assert(pthread_mutex_lock(&fb_fifo.wmutex) == 0);
		while (fb_fifo.fb_val[fb_fifo.wp] != 0)
			assert(pthread_cond_wait(&fb_fifo.wcond, &fb_fifo.wmutex) == 0);
		assert(pthread_mutex_unlock(&fb_fifo.wmutex) == 0);

		draw_frame(cr[fb_fifo.wp], ma, ha);

		assert(pthread_mutex_lock(&fb_fifo.rmutex) == 0);
		fb_fifo.fb_val[fb_fifo.wp++] = 1;
		assert(pthread_mutex_unlock(&fb_fifo.rmutex) == 0);
		assert(pthread_cond_signal(&fb_fifo.rcond) == 0);

		if (fb_fifo.wp >= FB_NUM)
			fb_fifo.wp = 0;
		
		ma += 2 * M_PI / (FRAME_RATE * periodic);
		if (ma >= 2 * M_PI)
			ma -= 2 * M_PI;
		ha += 2 * M_PI / 12 / (FRAME_RATE * periodic);
		if (ha >= 2 * M_PI)
			ha -= 2 * M_PI;
	}

    return 0;
}

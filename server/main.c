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

#include <cairo.h>

struct drm {
	int fd;
	// for display
	drmModeResPtr res;
	drmModeCrtcPtr *crtcs;
	drmModeConnectorPtr *connectors;
	drmModeEncoderPtr *encoders;
	drmModeCrtcPtr curr_crtc;
	drmModeConnectorPtr curr_connector;
	drmModeEncoderPtr curr_encoder;
	// for GPU
	struct radeon_bo_manager *bufmgr;
	struct radeon_cs_manager *csm;
	struct radeon_cs *cs;
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

struct cursor {
	int x;
	int y;
	int width;
	int height;
	cairo_t *cr;
	struct radeon_bo *bo;
} cursor = {0};

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

	/* invalid in KMS world
	drm_radeon_getparam_t gp;
	unsigned int gart_base;
	memset(&gp, 0, sizeof(gp));
	gp.param = RADEON_PARAM_GART_BASE;
	gp.value = &gart_base;
	assert(drmCommandWriteRead(fd, DRM_RADEON_GETPARAM, &gp, sizeof(gp)) == 0);
	printf("DRM RADEON GART BASE: %x\n", gart_base);
	//*/

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

	struct radeon_cs_manager *csm;
	assert((csm = radeon_cs_manager_gem_ctor(fd)) != NULL);

	struct radeon_cs *cs;
	assert((cs = radeon_cs_create(csm, RADEON_BUFFER_SIZE/4)) != NULL);

	drm.fd = fd;
	drm.res = dmrp;
	drm.crtcs = dmcp;
	drm.connectors = dmnp;
	drm.encoders = dmep;
	drm.curr_crtc = crtc;
	drm.curr_connector = connector;
	drm.curr_encoder = encoder;

	drm.bufmgr = bufmgr;
	drm.csm = csm;
}

cairo_t *create_buffer(int width, int height, struct radeon_bo **rbo)
{
    struct radeon_bo *bo;
	int depth = 24, bpp = 32;
	int pitch = width * (bpp / 8);
	int size = pitch * height;
    assert((bo = radeon_bo_open(drm.bufmgr, 0, size, 0x200, RADEON_GEM_DOMAIN_VRAM, 0)) != NULL);
	if (rbo)
		*rbo = bo;

    assert(radeon_bo_map(bo, 1) == 0);
    printf("map bo %x\n", (uint32_t)bo->ptr);
    memset(bo->ptr, 0, size);

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

cairo_t *create_framebuffer(void)
{
	static int i = 0;
	assert(i < FB_NUM);

	struct radeon_bo *bo;
	int width = drm.curr_crtc->mode.hdisplay;
	int height = drm.curr_crtc->mode.vdisplay;
	int depth = 24, bpp = 32;
	int pitch = width * (bpp / 8);
    cairo_t *cr = create_buffer(width, height, &bo);

	int fb_id;
    assert(drmModeAddFB(drm.fd, width, height, depth, bpp, pitch, bo->handle, &fb_id) == 0);
	fb_fifo.fb_ids[i++] = fb_id;

	return cr;
}

void create_cursor(void)
{
	cursor.width = 64;
	cursor.height = 64;
	cairo_t *cr = create_buffer(cursor.width, cursor.height, &cursor.bo);
	cursor.cr = cr;

	cairo_set_source_rgba (cr, 0.2, 0.2, 1, 0.5);
	cairo_rectangle (cr, 0, 0, cursor.width, cursor.height);
	cairo_fill (cr);

	assert(drmModeSetCursor(drm.fd, drm.curr_crtc->crtc_id, cursor.bo->handle, cursor.width, cursor.height) == 0);
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
	cairo_set_source_rgba (cr, 0.2, 1, 0.2, 0.6);
	cairo_set_line_width (cr, 4.0);
	cairo_arc (cr, xc, yc, minute_radius, 0, ma);
	cairo_line_to (cr, xc, yc);
	cairo_stroke (cr);
}

static void user_abort(int dummy)
{
	// restore previous framebuffer
	assert(drmModeSetCrtc(drm.fd, drm.curr_crtc->crtc_id, drm.curr_crtc->buffer_id, 
						  drm.curr_crtc->x, drm.curr_crtc->y, &drm.curr_connector->connector_id, 
						  1, &drm.curr_crtc->mode) == 0);

	// hide cursor
	assert(drmModeSetCursor(drm.fd, drm.curr_crtc->crtc_id, 0, cursor.width, cursor.height) == 0);
    
    drmClose(drm.fd);
    exit(0);
}

static void *frame_update(void *arg)
{
	// wait for the first frame ready
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

	int cursor_delta = 1;
	int old_rp = -1;
	while (1) {
		// make display frame the start of a periodic
		// the update will be more periodic aligned
		/*
		assert(drmModeSetCrtc(drm.fd, drm.curr_crtc->crtc_id, fb_fifo.fb_ids[fb_fifo.rp], 0, 0, 
							  &drm.curr_connector->connector_id, 1, &drm.curr_crtc->mode) == 0);
		//*/
		// pend a page flip request, generate a page flip event when complete
		assert(drmModePageFlip(drm.fd, drm.curr_crtc->crtc_id, fb_fifo.fb_ids[fb_fifo.rp], DRM_MODE_PAGE_FLIP_EVENT, 0) == 0);

		// move cursor
		assert(drmModeMoveCursor(drm.fd, drm.curr_crtc->crtc_id, cursor.x, cursor.y) == 0);
		cursor.x += cursor_delta;
		cursor.y += cursor_delta;
		if (cursor.x >= drm.curr_crtc->mode.hdisplay || 
			cursor.y >= drm.curr_crtc->mode.vdisplay ||
			cursor.x <= 0 || cursor.y <= 0)
			cursor_delta = -cursor_delta;

		// wait for page flip complete
		assert(drmHandleEvent(drm.fd, &evctx) == 0);

		// not until the page flip complete can we free the previous frame buffer
		if (old_rp >= 0) {
			assert(pthread_mutex_lock(&fb_fifo.wmutex) == 0);
			fb_fifo.fb_val[old_rp] = 0;
			assert(pthread_mutex_unlock(&fb_fifo.wmutex) == 0);
			assert(pthread_cond_signal(&fb_fifo.wcond) == 0);
		}

		old_rp = fb_fifo.rp++;
		if (fb_fifo.rp >= FB_NUM)
			fb_fifo.rp = 0;

		// wait for the next frame buffer paint ready
		assert(pthread_mutex_lock(&fb_fifo.rmutex) == 0);
		while (fb_fifo.fb_val[fb_fifo.rp] != 1)
			assert(pthread_cond_wait(&fb_fifo.rcond, &fb_fifo.rmutex) == 0);
		assert(pthread_mutex_unlock(&fb_fifo.rmutex) == 0);

		// sleep until the next frame
		t += delta;
		ts.tv_sec = t / 1000000000;
		ts.tv_nsec = t % 1000000000;
		assert(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == 0);
	}
}

int main()
{
    drm_init();
	create_cursor();

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
		// wait for the display thread free the frame buffer
		assert(pthread_mutex_lock(&fb_fifo.wmutex) == 0);
		while (fb_fifo.fb_val[fb_fifo.wp] != 0)
			assert(pthread_cond_wait(&fb_fifo.wcond, &fb_fifo.wmutex) == 0);
		assert(pthread_mutex_unlock(&fb_fifo.wmutex) == 0);

		// draw the frame buffer
		draw_frame(cr[fb_fifo.wp], ma, ha);

		// mark frame buffer ready
		assert(pthread_mutex_lock(&fb_fifo.rmutex) == 0);
		fb_fifo.fb_val[fb_fifo.wp++] = 1;
		assert(pthread_mutex_unlock(&fb_fifo.rmutex) == 0);
		assert(pthread_cond_signal(&fb_fifo.rcond) == 0);

		if (fb_fifo.wp >= FB_NUM)
			fb_fifo.wp = 0;

		// update clock state
		ma += 2 * M_PI / (FRAME_RATE * periodic);
		if (ma >= 2 * M_PI)
			ma -= 2 * M_PI;
		ha += 2 * M_PI / 12 / (FRAME_RATE * periodic);
		if (ha >= 2 * M_PI)
			ha -= 2 * M_PI;
	}

    return 0;
}

#include <stdio.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

Display *display;
int screen;
Window rootw;

int main(void)
{
    assert((display = XOpenDisplay(NULL)) != NULL);
    screen = DefaultScreen(display);
    rootw = RootWindow(display, screen);

    int rrevent, rrerror;
    assert(XRRQueryExtension(display, &rrevent, &rrerror));
    int major, minor;
    assert(XRRQueryVersion(display, &major, &minor));
    printf("RandR version %d.%d\n", major, minor);

    int nsizes;
    XRRScreenSize *rrsize = XRRSizes(display, screen, &nsizes);
    int i, j;
    for (i = 0; i < nsizes; i++)
	printf("RRSize %d %d %d %d\n", 
	       rrsize[i].width, rrsize[i].height, 
	       rrsize[i].mwidth, rrsize[i].mheight);

    XRRScreenResources *rrr = XRRGetScreenResources(display, rootw);

    for (i = 0; i < rrr->ncrtc; i++) {
	XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, rrr, rrr->crtcs[i]);
	printf("CRTC %d x=%d y=%d w=%d h=%d np=%d /p",
	       rrr->crtcs[i],
	       crtc_info->x, crtc_info->y,
	       crtc_info->width, crtc_info->height,
	       crtc_info->noutput);
	for (j = 0; j < crtc_info->noutput; j++)
	    printf(" %d", crtc_info->outputs[j]);
	printf(" /pp");
	for (j = 0; j < crtc_info->npossible; j++)
	    printf(" %d", crtc_info->possible[j]);
	printf("\n");
    }

    for (i = 0; i < rrr->noutput; i++) {
	XRROutputInfo *output_info = XRRGetOutputInfo(display, rrr, rrr->outputs[i]);
	printf("OUTPUT %d %s crtc=%d /c", 
	       rrr->outputs[i], output_info->name, 
	       output_info->crtc);
	for (j = 0; j < output_info->ncrtc; j++)
	    printf(" %d", output_info->crtcs[j]);
	printf(" /p");
	for (j = 0; j < output_info->nclone; j++)
	    printf(" %d", output_info->clones[j]);
	printf(" /m");
	for (j = 0; j < output_info->nmode; j++)
	    printf(" %d", output_info->modes[j]);
	printf("\n");
    }

    for (i = 0; i < rrr->nmode; i++) {
	printf("MODE %d %s w=%d h=%d\n",
	       rrr->modes[i].id, rrr->modes[i].name, rrr->modes[i].width, rrr->modes[i].height);
    }

    XRRSelectInput(display, rootw, RRScreenChangeNotifyMask|RRCrtcChangeNotifyMask|RROutputChangeNotifyMask|RROutputPropertyNotifyMask);

    XEvent event;
    while (1) {
	XNextEvent(display, &event);
	if (event.type == rrevent + RRScreenChangeNotify) {
	    XRRScreenChangeNotifyEvent *sce = (XRRScreenChangeNotifyEvent *)&event;
	    printf("ScreenChange w=%d h=%d mw=%d mh=%d\n", 
		   sce->width, sce->height, sce->mwidth, sce->mheight);
	}
	else if (event.type == rrevent + RRNotify + RRNotify_CrtcChange) {
	    XRROutputChangeNotifyEvent *oce = (XRROutputChangeNotifyEvent *)&event;
	    printf("OutputChange p=%d c=%d m=%d\n",
		   oce->output, oce->crtc, oce->mode);
	}
	else if (event.type == rrevent + RRNotify + RRNotify_OutputChange) {
	    XRRCrtcChangeNotifyEvent *cce = (XRRCrtcChangeNotifyEvent *)&event;
	    printf("CrtcChange c=%d m=%d x=%d y=%d w=%d h=%d\n", 
		   cce->crtc, cce->mode, cce->x, cce->y, cce->width, cce->height);
	}
	else if (event.type == rrevent + RRNotify + RRNotify_OutputProperty) {
	    XRROutputPropertyNotifyEvent *ope = (XRROutputPropertyNotifyEvent *)&event;
	    printf("OutputProperty p=%d\n", ope->output);
	}
    }

    XCloseDisplay(display);
    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <myx.h>

#define IMAGE_FILE "macross.bmp"

int main(int argc, char *argv[])
{
    myx_init();

    int image_width, image_height;
    Picture image_picture = image_get_picture(IMAGE_FILE, &image_width, &image_height);

    int screen=DefaultScreen(display);
    Window root=DefaultRootWindow(display);
    XRenderPictFormat *fmt=XRenderFindStandardFormat(display, PictStandardRGB24);

    Window window=XCreateWindow(display, root, 0, 0, 640, 480, 0,
				DefaultDepth(display, screen), InputOutput,
				DefaultVisual(display, screen), 
				0, NULL);

    XRenderPictureAttributes pict_attr;
    pict_attr.poly_edge=PolyEdgeSmooth;
    pict_attr.poly_mode=PolyModeImprecise;
    Picture picture=XRenderCreatePicture(display, window, fmt, CPPolyEdge|CPPolyMode, &pict_attr);
	
    XSelectInput(display, window, KeyPressMask|KeyReleaseMask|ExposureMask
		 |ButtonPressMask|StructureNotifyMask);
	
    XMapWindow(display, window);
    XRenderColor bg_color={.red=0xffff, .green=0xffff, .blue=0xffff, .alpha=0xffff};

    Picture trap_pen = create_pen(0xa000, 0x00, 0x1000, 0x8000);
    XTrapezoid trap[2];
    trap[0].top = 150 << 16;
    trap[0].bottom = 400 << 16;
    trap[0].left.p1.x = 150 << 16;
    trap[0].left.p1.y = 10 << 16;
    trap[0].left.p2.x = 150 << 16;
    trap[0].left.p2.y = 450 << 16;
    trap[0].right.p1.x = 400 << 16;
    trap[0].right.p1.y = 10 << 16;
    trap[0].right.p2.x = 400 << 16;
    trap[0].right.p2.y = 450 << 16;
    trap[1].top = 400 << 16;
    trap[1].bottom = 450 << 16;
    trap[1].left.p1.x = 150 << 16;
    trap[1].left.p1.y = 400 << 16;
    trap[1].left.p2.x = 200 << 16;
    trap[1].left.p2.y = 600 << 16;
    trap[1].right.p1.x = 400 << 16;
    trap[1].right.p1.y = 400 << 16;
    trap[1].right.p2.x = 100 << 16;
    trap[1].right.p2.y = 600 << 16;
	
    while(1) {
	XEvent event;
	XNextEvent(display, &event);
		
	switch(event.type) {
	case Expose:
	    XRenderFillRectangle(display, PictOpOver,
				 picture, &bg_color, 0, 0, 640, 480);
	    XRenderComposite(display, PictOpOver, image_picture, 0, picture, 0, 0, 0, 0, 0, 0, image_width, image_height);
	    XRenderCompositeTrapezoids(display,
				       PictOpOver, 
				       trap_pen, picture, 0,
				       0, 0, trap, 2);
	    glyph_draw_string(picture, 200, 240, "Hello world!");
	    break;
	case DestroyNotify:
	    return 0;
	}
    }
    
    myx_exit();
    return 0;
}

#include <cassert>
#include <map>
#include <iostream>

extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shapeconst.h>
}

using namespace std;

Display *display;
int screen;
Window rootw;
Window compw;
Picture compp;
struct win_record {
    int x;
    int y;
    int width;
    int height;
    int board;
    Window parent;
    Window child;
    Pixmap pixmap;
    Damage damage;
};
map<Window, win_record> windows;

void draw_dacoration(Window w)
{
    Window dw;
    XWindowAttributes wattr;
    win_record rec;
    int board = 5;
    assert(XGetWindowAttributes(display, w, &wattr));
    dw = XCreateSimpleWindow(display, rootw, wattr.x, wattr.y, 
			     wattr.width + 20, wattr.height + 30, 
			     board, BlackPixel(display, screen), 
			     WhitePixel(display, screen));
    XReparentWindow(display, w, dw, wattr.x + 10, wattr.y + 20);
    XMapRaised(display, dw);
    Pixmap pixmap = XCompositeNameWindowPixmap(display, dw);
    Damage damage = XDamageCreate(display, dw, XDamageReportRawRectangles);
    rec.x = wattr.x;
    rec.y = wattr.y;
    rec.width = wattr.width + 20;
    rec.height = wattr.height + 30;
    rec.board = board;
    rec.parent = dw;
    rec.child = w;
    rec.pixmap = pixmap;
    rec.damage = damage;
    windows[dw] = rec;
}

int main(void)
{
    XEvent event;
    assert((display = XOpenDisplay(NULL)) != NULL);
    screen = DefaultScreen(display);
    rootw = RootWindow(display, screen);

    int opcode, comp_event, comp_error;
    assert(XQueryExtension(display, "Composite", &opcode, &comp_event, &comp_error));
    assert(XCompositeQueryExtension(display, &comp_event, &comp_error));
    int major, minor;
    assert(XCompositeQueryVersion(display, &major, &minor));
    cout<<"X Composite extension version: "<<major<<'.'<<minor<<endl;
    XCompositeRedirectSubwindows (display, rootw, CompositeRedirectManual);
    //XCompositeRedirectSubwindows(display, rootw, CompositeRedirectAutomatic);
    compw = XCompositeGetOverlayWindow(display, rootw);
    XMapWindow(display, compw);
    XRenderPictFormat *fmt = XRenderFindStandardFormat(display, PictStandardRGB24);
    compp = XRenderCreatePicture(display, compw, fmt, 0, 0);

    int dmg_event, dmg_error;
    assert(XDamageQueryExtension(display, &dmg_event, &dmg_error));

    int fix_event, fix_error;
    assert(XFixesQueryExtension(display, &fix_event, &fix_error));
    assert(XFixesQueryVersion(display, &major, &minor));
    cout<<"X Fixes extension version: "<<major<<'.'<<minor<<endl;

    // make inputs pass through the composite overlay, can't redirect them
    XserverRegion region = XFixesCreateRegion(display, 0, 0);
    XFixesSetWindowShapeRegion(display, compw, ShapeBounding, 0, 0, 0);
    XFixesSetWindowShapeRegion(display, compw, ShapeInput, 0, 0, region);
    XFixesDestroyRegion(display, region);

    XSelectInput(display, rootw, SubstructureNotifyMask);

    GC pen;
    XGCValues values;
    values.line_width = 1;
    values.line_style = LineSolid;
    pen = XCreateGC(display, compw, GCLineWidth|GCLineStyle, &values);

    while (1) {
	XNextEvent(display, &event);
	switch (event.type) {
	case MapNotify:
	    XMapEvent *me = (XMapEvent *)&event;
	    if (!windows.count(me->window))
		draw_dacoration(me->window);
	    break;
	}
	if (event.type == dmg_event + XDamageNotify) {
	    XDamageNotifyEvent *de = (XDamageNotifyEvent *)&event;
	    map<Window, win_record>::iterator it = windows.find(de->drawable);
	    if (it == windows.end())
		cerr<<"unknow drawable "<<de->drawable<<endl;
	    else {
		/*
		XCopyArea(display, it->second.pixmap, compw, DefaultGC(display, 0), 
			  de->area.x, de->area.y, 
			  de->area.width, de->area.height, 
			  it->second.x + de->area.x + it->second.board, 
			  it->second.y + de->area.y + it->second.board);
		//*/
		//*
		XCopyArea(display, it->second.pixmap, compw, DefaultGC(display, 0), 
			  0, 0, it->second.width, it->second.height, 
			  it->second.x, it->second.y);
		XDrawRectangle(display, compw, pen, 
			       it->second.x + de->area.x + it->second.board, 
			       it->second.y + de->area.y + it->second.board, 
			       de->area.width, de->area.height);
		//*/
	    }
	}
	 
    }

    XCloseDisplay(display);
    return 0;
}


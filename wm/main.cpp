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

struct win_record {
    int x;
    int y;
    int width;
    int height;
    int board;
    Window parent;
    Window child;
    Pixmap pixmap;
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
    rec.x = wattr.x;
    rec.y = wattr.y;
    rec.width = wattr.width + 20;
    rec.height = wattr.height + 30;
    rec.board = board;
    rec.parent = dw;
    rec.child = w;
    rec.pixmap = pixmap;
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
    XCompositeRedirectSubwindows(display, rootw, CompositeRedirectAutomatic);
    //compw = XCompositeGetOverlayWindow(display, rootw);
    //XMapWindow(display, compw);

    XSelectInput(display, rootw, SubstructureNotifyMask);

    while (1) {
	XNextEvent(display, &event);
	switch (event.type) {
	case MapNotify:
	    XMapEvent *me = (XMapEvent *)&event;
	    if (!windows.count(me->window))
		draw_dacoration(me->window);
	    break;
	}
    }

    XCloseDisplay(display);
    return 0;
}


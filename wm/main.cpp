#include <cassert>
#include <map>

extern "C" {
#include <X11/Xlib.h>
}

using namespace std;

Display *display;
int screen;
Window rootw;
struct win_record {
    int x;
    int y;
    int width;
    int height;
};
map<Window, win_record> windows;

void draw_dacoration(Window w)
{
    Window dw;
    XWindowAttributes wattr;
    win_record rec;
    assert(XGetWindowAttributes(display, w, &wattr));
    dw = XCreateSimpleWindow(display, rootw, wattr.x, wattr.y, 
			     wattr.width + 20, wattr.height + 30, 
			     5, BlackPixel(display, screen), 
			     WhitePixel(display, screen));
    rec.x = wattr.x;
    rec.y = wattr.y;
    rec.width = wattr.width + 20;
    rec.height = wattr.height + 30;
    windows[dw] = rec;
    XReparentWindow(display, w, dw, wattr.x + 10, wattr.y + 20);
    XMapRaised(display, dw);
}

int main(void)
{
    XEvent event;
    assert((display = XOpenDisplay(NULL)) != NULL);
    screen = DefaultScreen(display);
    rootw = RootWindow(display, screen);
    XSelectInput(display, rootw, SubstructureNotifyMask);

    while (1) {
	XNextEvent(display, &event);
	switch (event.type) {
	case MapNotify:
	    XMapEvent *mape = (XMapEvent *)&event;
	    if (!windows.count(mape->window))
		draw_dacoration(mape->window);
	    break;
	}
    }

    XCloseDisplay(display);
    return 0;
}


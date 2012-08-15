#include <cassert>
#include <cstring>
#include <map>
#include <iostream>

extern "C" {
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shapeconst.h>
#include<GL/gl.h>
#include<GL/glx.h>
#include<GL/glu.h>
}

typedef void    (*GLXBindTexImageProc)    (Display	 *display,
					   GLXDrawable	 drawable,
					   int		 buffer,
					   int		 *attribList);
typedef void    (*GLXReleaseTexImageProc) (Display	 *display,
					   GLXDrawable	 drawable,
					   int		 buffer);

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

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
    Picture picture;
    GLXPixmap glxp;
    float top;
    float bottom;
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

    /*
    XVisualInfo tmpl, *vi;
    int nvi;
    tmpl.visualid = XVisualIDFromVisual(wattr.visual);
    assert(vi = XGetVisualInfo(display, VisualIDMask, &tmpl, &nvi));
    assert(nvi == 1);
    GLXPixmap glxp = glXCreateGLXPixmap(display, vi, pixmap);
    */

    VisualID visualid = XVisualIDFromVisual(wattr.visual);
    int nfbconfigs;
    GLXFBConfig *fbconfigs = glXGetFBConfigs(display, screen, &nfbconfigs);
    int i;
    float top, bottom;
    for (i = 0; i < nfbconfigs; i++) {
        XVisualInfo *visinfo = glXGetVisualFromFBConfig(display, fbconfigs[i]);
        if (!visinfo || visinfo->visualid != visualid)
            continue;

	int value;
        glXGetFBConfigAttrib(display, fbconfigs[i], GLX_DRAWABLE_TYPE, &value);
        if (!(value & GLX_PIXMAP_BIT))
            continue;

        glXGetFBConfigAttrib (display, fbconfigs[i],
                              GLX_BIND_TO_TEXTURE_TARGETS_EXT,
                              &value);
        if (!(value & GLX_TEXTURE_2D_BIT_EXT))
            continue;

        glXGetFBConfigAttrib (display, fbconfigs[i],
                              GLX_BIND_TO_TEXTURE_RGBA_EXT,
                              &value);
        if (value == FALSE) {
            glXGetFBConfigAttrib (display, fbconfigs[i],
                                  GLX_BIND_TO_TEXTURE_RGB_EXT,
                                  &value);
            if (value == FALSE)
                continue;
        }

        glXGetFBConfigAttrib (display, fbconfigs[i],
                              GLX_Y_INVERTED_EXT,
                              &value);
        if (value == TRUE) {
            top = 0.0f;
            bottom = 1.0f;
        }
        else {
            top = 1.0f;
            bottom = 0.0f;
        }

        break;
    }

    assert(i != nfbconfigs);
    int pixmapAttribs[] = { GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
			    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
			    None };
    GLXPixmap glxp = glXCreatePixmap(display, fbconfigs[i], pixmap, pixmapAttribs);

    XRenderPictFormat *picture_fmt = XRenderFindStandardFormat(display, PictStandardRGB24);
    XRenderPictureAttributes pict_attr;
    pict_attr.repeat = 1;
    Picture picture =  XRenderCreatePicture(display, pixmap, picture_fmt, CPRepeat, &pict_attr);
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
    rec.picture = picture;
    rec.glxp = glxp;
    rec.top = top;
    rec.bottom = bottom;
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

    int glx_event, glx_error;
    assert(glXQueryExtension(display, &glx_event, &glx_error));
    glXQueryVersion(display, &major, &minor);
    cout<<"X GLX extension version: "<<major<<'.'<<minor<<endl;

    XWindowAttributes wattr;
    assert(XGetWindowAttributes(display, compw, &wattr));

    XVisualInfo *vi;
    //*
    XVisualInfo tmpl;
    int nvi;
    tmpl.visualid = XVisualIDFromVisual(wattr.visual);
    assert(vi = XGetVisualInfo(display, VisualIDMask, &tmpl, &nvi));
    assert(nvi == 1);
    int isDoubleBuffer;
    glXGetConfig(display, vi, GLX_DOUBLEBUFFER, &isDoubleBuffer);
    if (!isDoubleBuffer)
	cerr<<"not a double buffered GL visual\n";
    //*/
    /*
    const int attribs[] = {
	GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
	GLX_RENDER_TYPE,   GLX_RGBA_BIT,
	GLX_RED_SIZE,      1,
	GLX_GREEN_SIZE,    1,
	GLX_BLUE_SIZE,     1,
	GLX_DOUBLEBUFFER,  GL_TRUE,
	GLX_DEPTH_SIZE,    1,
	None
    };
    int num_configs;
    GLXFBConfig *fbconfig = glXChooseFBConfig(display, screen, attribs, &num_configs);
    assert(fbconfig != NULL);
    assert(vi = glXGetVisualFromFBConfig(display, fbconfig[0]));
    //*/

    const char *glxExtensions = glXQueryExtensionsString(display, 0);
    cout<<glxExtensions<<endl;
    assert(strstr(glxExtensions, "GLX_EXT_texture_from_pixmap"));

    GLXBindTexImageProc bindTexImage = (GLXBindTexImageProc)glXGetProcAddressARB((GLubyte *)"glXBindTexImageEXT");
    GLXReleaseTexImageProc releaseTexImage = (GLXReleaseTexImageProc)glXGetProcAddressARB((GLubyte *)"glXReleaseTexImageEXT");

    GLXContext glc = glXCreateContext(display, vi, NULL, GL_TRUE);
    glXMakeCurrent(display, compw, glc);

    glViewport(0, 0, (GLsizei)wattr.width, (GLsizei)wattr.height);
    glClearColor(1.0, 1.0, 1.0, 0.0);
    glColor3f(1.0f, 1.0f, 0.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, wattr.width, 0, wattr.height);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable( GL_TEXTURE_2D );
    GLuint texture;
    glGenTextures (1, &texture);
    glBindTexture (GL_TEXTURE_2D, texture);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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

    XFlush(display);

    cout<<"loop"<<endl;
    while (1) {
	XNextEvent(display, &event);
	cout<<"event"<<endl;
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
	    cout<<"render"<<endl;
	    if (it == windows.end())
		cerr<<"unknow drawable "<<de->drawable<<endl;
	    else {
		/*
		XCopyArea(display, it->second.pixmap, compw, DefaultGC(display, 0), 
			  de->area.x + it->second.board, 
			  de->area.y + it->second.board, 
			  de->area.width, de->area.height, 
			  it->second.x + de->area.x + it->second.board, 
			  it->second.y + de->area.y + it->second.board);
		//*/
		/*
		XCopyArea(display, it->second.pixmap, compw, DefaultGC(display, 0), 
			  0, 0, it->second.width, it->second.height, 
			  it->second.x, it->second.y);
		XDrawRectangle(display, compw, pen, 
			       it->second.x + de->area.x + it->second.board, 
			       it->second.y + de->area.y + it->second.board, 
			       de->area.width, de->area.height);
		//*/
		/*/
		XRenderComposite(display, PictOpOver, it->second.picture, 0, compp, 
				 de->area.x + it->second.board, 
				 de->area.y + it->second.board, 
				 0, 0, 
				 it->second.x + de->area.x + it->second.board, 
				 it->second.y + de->area.y + it->second.board, 
				 de->area.width, de->area.height);
		//*/
		//*/
		glClear(GL_COLOR_BUFFER_BIT);
		glClear(GL_DEPTH_BUFFER_BIT);
		
		bindTexImage(display, it->second.glxp, GLX_FRONT_LEFT_EXT, NULL);
		glBegin (GL_QUADS);
		glTexCoord2d (0.0f, it->second.bottom);
		glVertex2d (0.0f, 0.0f);
		glTexCoord2d (0.0f, it->second.top);
		glVertex2d (0.0f, 1.0f * it->second.height);
		glTexCoord2d (1.0f, it->second.top);
		glVertex2d (1.0f * it->second.width, 1.0f * it->second.height);
		glTexCoord2d (1.0f, it->second.bottom);
		glVertex2d (1.0f * it->second.width, 0.0f);
		glEnd ();
		releaseTexImage(display, it->second.glxp, GLX_FRONT_LEFT_EXT);

		/*
		glColor3f(0, 0, 1.0);
		glBegin(GL_QUADS);
		glVertex2d(10.0, 10.0);
		glVertex2d(10.0, 120.0);
		glVertex2d(120.0, 120.0);
		glVertex2d(120.0, 10.0);
		glEnd();
		*/

		glXSwapBuffers(display, compw);
		//*/
	    }
	}
	 
    }

    XCloseDisplay(display);
    return 0;
}


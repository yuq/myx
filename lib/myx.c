#include <assert.h>
#include <myx.h>

Display *display;

void myx_init(void)
{
    assert((display = XOpenDisplay(NULL)) != NULL);
    glyph_init();
    image_init();
}


void myx_exit(void)
{
    image_exit();
    glyph_exit();
    XCloseDisplay(display);
}




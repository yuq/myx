#ifndef _MYX_H
#define _MYX_H

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

extern Display *display;

void glyph_init(void);
void glyph_exit(void);
void glyph_draw_string(Picture dst, int x, int y, char *str);
Picture create_pen(int red, int green, int blue, int alpha);

void image_init(void);
void image_exit(void);
Picture image_get_picture(const char *file, int *width, int *height);

void myx_init(void);
void myx_init(void);

#endif

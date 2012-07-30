#include <assert.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <myx.h>

static void load_glyph(GlyphSet gs, FT_Face face, int charcode)
{
	Glyph gid;
	XGlyphInfo ginfo;
	
	int glyph_index=FT_Get_Char_Index(face, charcode);
	FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
	
	FT_Bitmap *bitmap=&face->glyph->bitmap;
	ginfo.x=-face->glyph->bitmap_left;
	ginfo.y=face->glyph->bitmap_top;
	ginfo.width=bitmap->width;
	ginfo.height=bitmap->rows;
	ginfo.xOff=face->glyph->advance.x/64;
	ginfo.yOff=face->glyph->advance.y/64;
	
	gid=charcode;
	
	int stride=(ginfo.width+3)&~3;
	char tmpbitmap[stride*ginfo.height];
	int y;
	for(y=0; y<ginfo.height; y++)
		memcpy(tmpbitmap+y*stride, bitmap->buffer+y*ginfo.width, ginfo.width);
	
	XRenderAddGlyphs(display, gs, &gid, &ginfo, 1,
		tmpbitmap, stride*ginfo.height);
	XSync(display, 0);
}

static GlyphSet load_glyphset(char *filename, int size)
{
	static int ft_lib_initialized=0;
	static FT_Library library;
	int n;
	XRenderPictFormat *fmt_a8;
	GlyphSet gs;
	FT_Face face;

	fmt_a8 = XRenderFindStandardFormat(display, PictStandardA8);
	gs = XRenderCreateGlyphSet(display, fmt_a8);

	if (!ft_lib_initialized)
	    assert(!FT_Init_FreeType(&library));
	
	assert(!FT_New_Face(library, filename, 0, &face));
	
	FT_Set_Char_Size(face, 0, size*64, 90, 90);
	
	for(n=32; n<128; n++)
		load_glyph(gs, face, n);
	
	FT_Done_Face(face);
	
	return gs;
}

Picture create_pen(int red, int green, int blue, int alpha)
{
	XRenderColor color;
	color.red=red;
	color.green=green;
	color.blue=blue;
	color.alpha=alpha;
	XRenderPictFormat *fmt=XRenderFindStandardFormat(display, PictStandardARGB32);
	
	Window root=DefaultRootWindow(display);
	Pixmap pm=XCreatePixmap(display, root, 1, 1, 32);
	XRenderPictureAttributes pict_attr;
	pict_attr.repeat=1;
	Picture picture=XRenderCreatePicture(display, pm, fmt, CPRepeat, &pict_attr);
	XRenderFillRectangle(display, PictOpOver, picture, &color, 0, 0, 1, 1);
	XFreePixmap(display, pm);
	return picture;
}

static GlyphSet font;
static Picture pen;

#define FONT_FILE "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSerif.ttf"
#define FONT_SIZE 24

void glyph_init(void)
{
    font = load_glyphset(FONT_FILE, FONT_SIZE);
    pen=create_pen(0,0,0,0xffff);
}

void glyph_exit(void)
{
    
}

void glyph_draw_string(Picture dst, int x, int y, char *str)
{
    XRenderCompositeString8(display, PictOpOver,
			    pen, dst, 0,
			    font, 0, 0, x, y, str, strlen(str));
}



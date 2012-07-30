#include <assert.h>
#include <stdio.h>
#include <myx.h>
#include <FreeImage.h>


Picture image_get_picture(const char *file, int *width, int *height)
{
    FIBITMAP *bitmap = FreeImage_Load(FIF_BMP, file, BMP_DEFAULT);
    assert(bitmap != NULL);
    FREE_IMAGE_TYPE image_type = FreeImage_GetImageType(bitmap);
    unsigned image_bpp = FreeImage_GetBPP(bitmap);
    unsigned image_width = FreeImage_GetWidth(bitmap);
    unsigned image_height = FreeImage_GetHeight(bitmap);
    *width = image_width;
    *height = image_height;
    printf("iamge type=%d bpp=%u width=%u height=%u\n", 
	   image_type, image_bpp, image_width, image_height);
    Pixmap image_pixmap = XCreatePixmap(display, DefaultRootWindow(display), image_width, image_height, image_bpp);
    
    XImage image;
    image.width = image_width;
    image.height = image_height;
    image.format = ZPixmap;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 8;
    image.bitmap_bit_order = LSBFirst;
    image.red_mask = FreeImage_GetRedMask(bitmap);
    image.green_mask = FreeImage_GetGreenMask(bitmap);
    image.blue_mask = FreeImage_GetBlueMask(bitmap);
    image.xoffset = 0;
    image.bitmap_pad = 32;
    image.depth = image_bpp;
    image.data = FreeImage_GetBits(bitmap);
    image.bits_per_pixel = image_bpp;
    image.bytes_per_line = FreeImage_GetPitch(bitmap);
    image.obdata = NULL;
    XInitImage(&image);
    GC image_gc = XCreateGC(display, image_pixmap, 0, NULL);
    XPutImage(display, image_pixmap, image_gc, &image, 0, 0, 0, 0, image_width, image_height);
    XRenderPictFormat *image_picture_fmt = XRenderFindStandardFormat(display, PictStandardRGB24);
    XRenderPictureAttributes pict_attr;
    pict_attr.repeat=1;
    Picture image_picture =  XRenderCreatePicture(display, image_pixmap, image_picture_fmt, CPRepeat, &pict_attr);

    XTransform trans = {{
	    {XDoubleToFixed(1.0), 0, 0},
	    {0, XDoubleToFixed(-1.0), 0},
	    {0, 0, XDoubleToFixed(1.0)},
	}};
    trans.matrix[1][2] = image_height << 16;
    XRenderSetPictureTransform(display, image_picture, &trans);
    XSync(display, 0);
    FreeImage_Unload(bitmap);
    return image_picture;
}

void image_init(void)
{
    FreeImage_Initialise(FALSE);
}

void image_exit(void)
{
    FreeImage_DeInitialise();
}



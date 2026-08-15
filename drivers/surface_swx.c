#include "surface_driver.h"

#include "log.h"
#include "mem.h"

typedef void Display;
typedef void *GC;
typedef unsigned long Window;
typedef unsigned long VisualID;

typedef struct XImage_s XImage;

typedef struct Visual_s {
	void *ext_data;
	VisualID visualid;
	int class;
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	int bits_per_rgb;
	int map_entries;
} Visual;

typedef struct XImageFuncs_s {
	void *create_image;
	int (*destroy_image)(XImage *);
	unsigned long (*get_pixel)(XImage *, int, int);
	int (*put_pixel)(XImage *, int, int, unsigned long);
	XImage *(*sub_image)(XImage *, int, int, unsigned int, unsigned int);
	int (*add_pixel)(XImage *, long);
} XImageFuncs;

struct XImage_s {
	int width;
	int height;
	int xoffset;
	int format;
	char *data;
	int byte_order;
	int bitmap_unit;
	int bitmap_bit_order;
	int bitmap_pad;
	int depth;
	int bytes_per_line;
	int bits_per_pixel;
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	void *obdata;
	XImageFuncs f;
};

enum {
	X_Z_PIXMAP	      = 2,
	X_LSB_FIRST	      = 0,
	SURFACE_SWX_IMAGE_PAD = 32,
};

typedef struct swx_s {
	Visual *(*DefaultVisual)(Display *, int);
	int (*DefaultDepth)(Display *, int);
	GC (*DefaultGC)(Display *, int);
	VisualID (*VisualIDFromVisual)(Visual *);
	XImage *(*CreateImage)(Display *, Visual *, unsigned int, int, int, char *, unsigned int, unsigned int, int, int);
	int (*DestroyImage)(XImage *);
	int (*PutImage)(Display *, Window, GC, XImage *, int, int, int, int, unsigned int, unsigned int);
	int (*Flush)(Display *);
} swx_t;

typedef struct surface_swx_channel_s {
	unsigned int shift;
	unsigned int bits;
	unsigned long max;
} surface_swx_channel_t;

typedef struct surface_swx_format_s {
	surface_swx_channel_t red;
	surface_swx_channel_t green;
	surface_swx_channel_t blue;
	u32 byte_count;
	int lsb_first;
} surface_swx_format_t;

typedef struct surface_swx_s {
	proc_t *proc;
	alloc_t alloc;
	void *lib;
	swx_t swx;
	Display *display;
	int screen;
	Visual *visual;
	int depth;
	GC gc;
	Window window;
	u8 *pixels;
	void *image_data;
	size_t pixels_size;
	size_t image_size;
	u16 width;
	u16 height;
	XImage *image;
	surface_swx_format_t format;
	gfx_surface_t gfx_surface;
} surface_swx_t;

static int surface_swx_load_symbol(surface_swx_t *ctx, void **sym, strv_t name)
{
	if (proc_dlsym(ctx->proc, ctx->lib, name, sym)) {
		log_error("csurface", "swx", NULL, "failed to load X11 symbol: %.*s", name.len, name.data);
		return 1;
	}

	return 0;
}

#define LOAD_X11(_ctx, _name) surface_swx_load_symbol((_ctx), (void **)&(_ctx)->swx._name, STRV("X" #_name))

static int surface_swx_load(surface_swx_t *ctx, proc_t *proc)
{
	ctx->proc = proc;
	if (proc_dlopen(ctx->proc, STRV("libX11.so.6"), &ctx->lib) && proc_dlopen(ctx->proc, STRV("libX11.so"), &ctx->lib)) {
		log_error("csurface", "swx", NULL, "failed to load libX11.so");
		return 1;
	}

	if (LOAD_X11(ctx, DefaultVisual) || LOAD_X11(ctx, DefaultDepth) || LOAD_X11(ctx, DefaultGC) || LOAD_X11(ctx, VisualIDFromVisual) ||
	    LOAD_X11(ctx, CreateImage) || LOAD_X11(ctx, DestroyImage) || LOAD_X11(ctx, PutImage) || LOAD_X11(ctx, Flush)) {
		proc_dlclose(ctx->proc, ctx->lib);
		ctx->lib = NULL;
		mem_set(&ctx->swx, 0, sizeof(ctx->swx));
		return 1;
	}

	return 0;
}

#undef LOAD_X11

static int surface_swx_compatible(const surface_info_t *info)
{
	return info != NULL && info->gfx_api == GFX_API_SOFTWARE && info->native_type == DISPLAY_NATIVE_X11;
}

static int surface_swx_init(surface_t *srf, const surface_config_t *config)
{
	if (srf == NULL || config == NULL || config->display == NULL || config->display->proc == NULL) {
		return 1;
	}

	surface_swx_t *ctx = alloc_alloc(&srf->alloc, sizeof(*ctx));
	if (ctx == NULL) {
		log_error("csurface", "swx", NULL, "failed to allocate surface data");
		return 1;
	}
	mem_set(ctx, 0, sizeof(*ctx));

	if (surface_swx_load(ctx, config->display->proc)) {
		alloc_free(&srf->alloc, ctx, sizeof(*ctx));
		return 1;
	}

	ctx->alloc = srf->alloc;
	srf->data  = ctx;
	return 0;
}

static void surface_swx_free_image(surface_swx_t *ctx)
{
	if (ctx->image != NULL) {
		ctx->image->data = NULL;
		ctx->swx.DestroyImage(ctx->image);
	}
	if (ctx->image_data != NULL) {
		alloc_free(&ctx->alloc, ctx->image_data, ctx->image_size);
	}
	if (ctx->pixels != NULL) {
		alloc_free(&ctx->alloc, ctx->pixels, ctx->pixels_size);
	}

	ctx->pixels	 = NULL;
	ctx->image_data	 = NULL;
	ctx->pixels_size = 0;
	ctx->image_size	 = 0;
	ctx->width	 = 0;
	ctx->height	 = 0;
	ctx->image	 = NULL;
	ctx->format	 = (surface_swx_format_t){0};
}

static int surface_swx_unbind(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_swx_t *ctx = srf->data;
	surface_swx_free_image(ctx);
	ctx->window	 = 0;
	ctx->gc		 = NULL;
	ctx->gfx_surface = (gfx_surface_t){0};
	return 0;
}

static int surface_swx_free(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_swx_t *ctx = srf->data;
	surface_swx_unbind(srf);
	if (ctx->lib != NULL) {
		proc_dlclose(ctx->proc, ctx->lib);
	}
	alloc_free(&srf->alloc, ctx, sizeof(*ctx));
	srf->data = NULL;
	return 0;
}

static int surface_swx_native_display(surface_t *srf, surface_swx_t *ctx)
{
	display_native_t native = {0};
	if (display_native(srf->config.display, &native) || native.type != DISPLAY_NATIVE_X11 || native.display == NULL) {
		log_error("csurface", "swx", NULL, "X11 native display is unavailable");
		return 1;
	}

	ctx->display = native.display;
	ctx->screen  = native.screen;
	ctx->visual  = ctx->swx.DefaultVisual(ctx->display, ctx->screen);
	ctx->depth   = ctx->swx.DefaultDepth(ctx->display, ctx->screen);
	ctx->gc	     = ctx->swx.DefaultGC(ctx->display, ctx->screen);
	if (ctx->visual == NULL || ctx->depth == 0 || ctx->gc == NULL) {
		log_error("csurface", "swx", NULL, "X11 default visual is unavailable");
		return 1;
	}

	return 0;
}

static int surface_swx_config_window(surface_t *srf, window_config_t *config)
{
	if (srf == NULL || srf->data == NULL || config == NULL) {
		return 1;
	}

	surface_swx_t *ctx = srf->data;
	if (surface_swx_native_display(srf, ctx)) {
		return 1;
	}

	config->depth	   = (u8)ctx->depth;
	config->visual	   = (u32)ctx->swx.VisualIDFromVisual(ctx->visual);
	config->background = WINDOW_BACKGROUND_NONE;
	return 0;
}

static const gfx_surface_ops_t surface_swx_gfx_ops;

static int surface_swx_bind(surface_t *srf, window_t *window)
{
	if (srf == NULL || srf->data == NULL || window == NULL) {
		return 1;
	}

	surface_swx_t *ctx = srf->data;
	if (surface_swx_native_display(srf, ctx)) {
		return 1;
	}

	window_native_t native = {0};
	if (window_native(window, &native) || native.type != DISPLAY_NATIVE_X11 || native.window == NULL) {
		log_error("csurface", "swx", NULL, "X11 native window is unavailable");
		return 1;
	}

	if (ctx->window != 0) {
		surface_swx_unbind(srf);
	}

	ctx->window	 = (Window)(uintptr_t)native.window;
	ctx->gfx_surface = (gfx_surface_t){
		.api	= GFX_API_SOFTWARE,
		.handle = ctx->window,
		.data	= ctx,
		.ops	= &surface_swx_gfx_ops,
	};
	return 0;
}

static unsigned int mask_shift(unsigned long mask)
{
	unsigned int shift = 0;
	while (mask != 0 && (mask & 1u) == 0) {
		mask >>= 1;
		shift++;
	}
	return shift;
}

static unsigned int mask_bits(unsigned long mask)
{
	unsigned int bits = 0;
	mask >>= mask_shift(mask);
	while ((mask & 1u) != 0) {
		mask >>= 1;
		bits++;
	}
	return bits;
}

static surface_swx_channel_t surface_swx_channel(unsigned long mask)
{
	unsigned int bits = mask_bits(mask);
	return (surface_swx_channel_t){
		.shift = mask_shift(mask),
		.bits  = bits,
		.max   = bits >= sizeof(unsigned long) * 8 ? ~0ul : (1ul << bits) - 1ul,
	};
}

static surface_swx_format_t surface_swx_format(const XImage *image)
{
	return (surface_swx_format_t){
		.red	    = surface_swx_channel(image->red_mask),
		.green	    = surface_swx_channel(image->green_mask),
		.blue	    = surface_swx_channel(image->blue_mask),
		.byte_count = (u32)(image->bits_per_pixel + 7) / 8,
		.lsb_first  = image->byte_order == X_LSB_FIRST,
	};
}

static unsigned long surface_swx_channel_pixel(u8 value, const surface_swx_channel_t *channel)
{
	if (channel->bits == 0) {
		return 0;
	}
	return ((unsigned long)value * channel->max / 255ul) << channel->shift;
}

static void surface_swx_write_pixel(const surface_swx_format_t *format, void *image_data, size_t offset, unsigned long pixel)
{
	u8 *data = (u8 *)image_data + offset;
	for (u32 i = 0; i < format->byte_count; i++) {
		u32 byte   = format->lsb_first ? i : format->byte_count - i - 1;
		data[byte] = (u8)(pixel >> (i * 8));
	}
}

static void surface_swx_convert(surface_swx_t *ctx)
{
	for (u16 y = 0; y < ctx->height; y++) {
		const u8 *src = ctx->pixels + (size_t)y * ctx->width * 4;
		for (u16 x = 0; x < ctx->width; x++) {
			unsigned long pixel = surface_swx_channel_pixel(src[0], &ctx->format.red) |
					      surface_swx_channel_pixel(src[1], &ctx->format.green) |
					      surface_swx_channel_pixel(src[2], &ctx->format.blue);
			surface_swx_write_pixel(&ctx->format,
						ctx->image_data,
						(size_t)y * ctx->image->bytes_per_line + (size_t)x * ctx->format.byte_count,
						pixel);
			src += 4;
		}
	}
}

static int surface_swx_gfx_memory(gfx_surface_t *surface, gfx_surface_memory_t *memory)
{
	if (surface == NULL || surface->data == NULL || memory == NULL || memory->width == 0 || memory->height == 0) {
		return 1;
	}

	surface_swx_t *ctx = surface->data;
	if (ctx->display == NULL || ctx->visual == NULL || ctx->window == 0) {
		return 1;
	}

	if (ctx->image != NULL && ctx->width == memory->width && ctx->height == memory->height) {
		memory->format = GFX_FORMAT_RGBA8;
		memory->data   = ctx->pixels;
		memory->stride = (size_t)ctx->width * 4;
		return 0;
	}

	surface_swx_free_image(ctx);

	size_t pixels_size = (size_t)memory->width * memory->height * 4;
	u8 *pixels	   = alloc_alloc(&ctx->alloc, pixels_size);
	if (pixels == NULL) {
		log_error("csurface", "swx", NULL, "failed to allocate surface pixels");
		return 1;
	}
	mem_set(pixels, 0, pixels_size);

	XImage *image = ctx->swx.CreateImage(ctx->display,
					     ctx->visual,
					     (unsigned int)ctx->depth,
					     X_Z_PIXMAP,
					     0,
					     NULL,
					     memory->width,
					     memory->height,
					     SURFACE_SWX_IMAGE_PAD,
					     0);
	if (image == NULL || image->bits_per_pixel <= 0 || image->bytes_per_line <= 0) {
		if (image != NULL) {
			ctx->swx.DestroyImage(image);
		}
		alloc_free(&ctx->alloc, pixels, pixels_size);
		log_error("csurface", "swx", NULL, "failed to create X11 image");
		return 1;
	}

	size_t image_size = (size_t)image->bytes_per_line * memory->height;
	void *image_data  = alloc_alloc(&ctx->alloc, image_size);
	if (image_data == NULL) {
		ctx->swx.DestroyImage(image);
		alloc_free(&ctx->alloc, pixels, pixels_size);
		log_error("csurface", "swx", NULL, "failed to allocate X11 image pixels");
		return 1;
	}
	mem_set(image_data, 0, image_size);
	image->data = image_data;

	ctx->pixels	 = pixels;
	ctx->image_data	 = image_data;
	ctx->pixels_size = pixels_size;
	ctx->image_size	 = image_size;
	ctx->width	 = memory->width;
	ctx->height	 = memory->height;
	ctx->image	 = image;
	ctx->format	 = surface_swx_format(image);

	memory->format = GFX_FORMAT_RGBA8;
	memory->data   = ctx->pixels;
	memory->stride = (size_t)ctx->width * 4;
	return 0;
}

static int surface_swx_gfx_present(gfx_surface_t *surface, gfx_present_mode_t present_mode)
{
	(void)present_mode;

	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_swx_t *ctx = surface->data;
	if (ctx->display == NULL || ctx->window == 0 || ctx->gc == NULL || ctx->image == NULL || ctx->pixels == NULL) {
		return 1;
	}

	surface_swx_convert(ctx);
	ctx->swx.PutImage(ctx->display, ctx->window, ctx->gc, ctx->image, 0, 0, 0, 0, ctx->width, ctx->height);
	return !ctx->swx.Flush(ctx->display);
}

static const gfx_surface_ops_t surface_swx_gfx_ops = {
	.present = surface_swx_gfx_present,
	.memory	 = surface_swx_gfx_memory,
};

static int surface_swx_native(surface_t *srf, surface_native_t *native)
{
	if (srf == NULL || srf->data == NULL || native == NULL) {
		return 1;
	}

	surface_swx_t *ctx = srf->data;
	if (ctx->display == NULL || ctx->visual == NULL || ctx->window == 0) {
		return 1;
	}

	*native = (surface_native_t){
		.gfx_api     = GFX_API_SOFTWARE,
		.native_type = DISPLAY_NATIVE_X11,
		.display     = ctx->display,
		.visual	     = ctx->visual,
		.handle	     = ctx->window,
		.gfx_surface = &ctx->gfx_surface,
	};
	return 0;
}

static surface_driver_t surface_swx = {
	.name		= "swx",
	.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND,
	.compatible	= surface_swx_compatible,
	.init		= surface_swx_init,
	.free		= surface_swx_free,
	.config_window	= surface_swx_config_window,
	.bind		= surface_swx_bind,
	.unbind		= surface_swx_unbind,
	.native		= surface_swx_native,
};

SURFACE_DRIVER(surface_swx, &surface_swx);

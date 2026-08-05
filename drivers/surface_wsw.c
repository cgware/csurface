#include "surface_driver.h"

#include "log.h"
#include "mem.h"

typedef void *HANDLE;
typedef HANDLE HBITMAP;
typedef HANDLE HDC;
typedef HANDLE HGDIOBJ;
typedef void *HWND;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef long LONG;
typedef int BOOL;

typedef struct BITMAPINFOHEADER_s {
	DWORD biSize;
	LONG biWidth;
	LONG biHeight;
	unsigned short biPlanes;
	unsigned short biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG biXPelsPerMeter;
	LONG biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct RGBQUAD_s {
	unsigned char rgbBlue;
	unsigned char rgbGreen;
	unsigned char rgbRed;
	unsigned char rgbReserved;
} RGBQUAD;

typedef struct BITMAPINFO_s {
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD bmiColors[1];
} BITMAPINFO;

enum {
	BI_RGB			= 0,
	DIB_RGB_COLORS		= 0,
	SRCCOPY			= 0x00CC0020,
	SURFACE_WSW_BIT_COUNT	= 32,
};

typedef struct wsw_s {
	HDC (*GetDC)(HWND);
	int (*ReleaseDC)(HWND, HDC);
	HDC (*CreateCompatibleDC)(HDC);
	BOOL (*DeleteDC)(HDC);
	HBITMAP (*CreateDIBSection)(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD);
	HGDIOBJ (*SelectObject)(HDC, HGDIOBJ);
	BOOL (*DeleteObject)(HGDIOBJ);
	BOOL (*BitBlt)(HDC, int, int, int, int, HDC, int, int, DWORD);
} wsw_t;

typedef struct surface_wsw_s {
	proc_t *proc;
	alloc_t alloc;
	void *user32;
	void *gdi32;
	wsw_t wsw;
	HWND window;
	HDC dc;
	HDC memory_dc;
	HBITMAP bitmap;
	HGDIOBJ old_bitmap;
	u8 *pixels;
	u8 *bitmap_pixels;
	size_t pixels_size;
	u16 width;
	u16 height;
	gfx_surface_t gfx_surface;
} surface_wsw_t;

static int surface_wsw_load_symbol(surface_wsw_t *ctx, void *lib, void **sym, strv_t name)
{
	if (proc_dlsym(ctx->proc, lib, name, sym)) {
		log_error("csurface", "wsw", NULL, "failed to load Win32 symbol: %.*s", name.len, name.data);
		return 1;
	}

	return 0;
}

#define LOAD_USER32(_ctx, _name) surface_wsw_load_symbol((_ctx), (_ctx)->user32, (void **)&(_ctx)->wsw._name, STRV(#_name))
#define LOAD_GDI32(_ctx, _name)	 surface_wsw_load_symbol((_ctx), (_ctx)->gdi32, (void **)&(_ctx)->wsw._name, STRV(#_name))

static void surface_wsw_unload(surface_wsw_t *ctx)
{
	if (ctx->gdi32 != NULL) {
		proc_dlclose(ctx->proc, ctx->gdi32);
		ctx->gdi32 = NULL;
	}
	if (ctx->user32 != NULL) {
		proc_dlclose(ctx->proc, ctx->user32);
		ctx->user32 = NULL;
	}
}

static int surface_wsw_load(surface_wsw_t *ctx, proc_t *proc)
{
	ctx->proc = proc;
	if (proc_dlopen(ctx->proc, STRV("user32.dll"), &ctx->user32)) {
		log_error("csurface", "wsw", NULL, "failed to load user32.dll");
		return 1;
	}
	if (proc_dlopen(ctx->proc, STRV("gdi32.dll"), &ctx->gdi32)) {
		log_error("csurface", "wsw", NULL, "failed to load gdi32.dll");
		surface_wsw_unload(ctx);
		return 1;
	}

	if (LOAD_USER32(ctx, GetDC) || LOAD_USER32(ctx, ReleaseDC) || LOAD_GDI32(ctx, CreateCompatibleDC) ||
	    LOAD_GDI32(ctx, DeleteDC) || LOAD_GDI32(ctx, CreateDIBSection) || LOAD_GDI32(ctx, SelectObject) ||
	    LOAD_GDI32(ctx, DeleteObject) || LOAD_GDI32(ctx, BitBlt)) {
		surface_wsw_unload(ctx);
		mem_set(&ctx->wsw, 0, sizeof(ctx->wsw));
		return 1;
	}

	return 0;
}

static int surface_wsw_compatible(const surface_info_t *info)
{
	return info != NULL && info->gfx_api == GFX_API_SOFTWARE && info->native_type == DISPLAY_NATIVE_WINDOWS;
}

static int surface_wsw_init(surface_t *srf, const surface_config_t *config)
{
	if (srf == NULL || config == NULL || config->display == NULL || config->display->proc == NULL) {
		return 1;
	}

	surface_wsw_t *ctx = alloc_alloc(&srf->alloc, sizeof(*ctx));
	if (ctx == NULL) {
		log_error("csurface", "wsw", NULL, "failed to allocate surface data");
		return 1;
	}
	mem_set(ctx, 0, sizeof(*ctx));

	if (surface_wsw_load(ctx, config->display->proc)) {
		alloc_free(&srf->alloc, ctx, sizeof(*ctx));
		return 1;
	}

	ctx->alloc = srf->alloc;
	srf->data  = ctx;
	return 0;
}

static void surface_wsw_free_bitmap(surface_wsw_t *ctx)
{
	if (ctx->memory_dc != NULL && ctx->old_bitmap != NULL) {
		ctx->wsw.SelectObject(ctx->memory_dc, ctx->old_bitmap);
	}
	if (ctx->bitmap != NULL) {
		ctx->wsw.DeleteObject(ctx->bitmap);
	}
	if (ctx->memory_dc != NULL) {
		ctx->wsw.DeleteDC(ctx->memory_dc);
	}
	if (ctx->pixels != NULL) {
		alloc_free(&ctx->alloc, ctx->pixels, ctx->pixels_size);
	}

	ctx->memory_dc     = NULL;
	ctx->bitmap	     = NULL;
	ctx->old_bitmap    = NULL;
	ctx->pixels	     = NULL;
	ctx->bitmap_pixels = NULL;
	ctx->pixels_size   = 0;
	ctx->width	     = 0;
	ctx->height	     = 0;
}

static int surface_wsw_unbind(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_wsw_t *ctx = srf->data;
	surface_wsw_free_bitmap(ctx);
	if (ctx->dc != NULL) {
		ctx->wsw.ReleaseDC(ctx->window, ctx->dc);
	}
	ctx->window	  = NULL;
	ctx->dc		  = NULL;
	ctx->gfx_surface = (gfx_surface_t){0};
	return 0;
}

static int surface_wsw_free(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_wsw_t *ctx = srf->data;
	surface_wsw_unbind(srf);
	surface_wsw_unload(ctx);
	alloc_free(&srf->alloc, ctx, sizeof(*ctx));
	srf->data = NULL;
	return 0;
}

static int surface_wsw_config_window(surface_t *srf, window_config_t *config)
{
	if (srf == NULL || srf->data == NULL || config == NULL) {
		return 1;
	}

	display_native_t native = {0};
	if (display_native(srf->config.display, &native) || native.type != DISPLAY_NATIVE_WINDOWS || native.display == NULL) {
		log_error("csurface", "wsw", NULL, "Windows native display is unavailable");
		return 1;
	}

	config->depth	   = 0;
	config->visual	   = 0;
	config->background = WINDOW_BACKGROUND_NONE;
	return 0;
}

static const gfx_surface_ops_t surface_wsw_gfx_ops;

static int surface_wsw_bind(surface_t *srf, window_t *window)
{
	if (srf == NULL || srf->data == NULL || window == NULL) {
		return 1;
	}

	display_native_t native_display = {0};
	if (display_native(srf->config.display, &native_display) || native_display.type != DISPLAY_NATIVE_WINDOWS ||
	    native_display.display == NULL) {
		log_error("csurface", "wsw", NULL, "Windows native display is unavailable");
		return 1;
	}

	window_native_t native_window = {0};
	if (window_native(window, &native_window) || native_window.type != DISPLAY_NATIVE_WINDOWS || native_window.window == NULL) {
		log_error("csurface", "wsw", NULL, "Windows native window is unavailable");
		return 1;
	}

	surface_wsw_t *ctx = srf->data;
	if (ctx->window != NULL) {
		surface_wsw_unbind(srf);
	}

	HWND hwnd = native_window.window;
	HDC dc	  = ctx->wsw.GetDC(hwnd);
	if (dc == NULL) {
		log_error("csurface", "wsw", NULL, "failed to get a Windows device context");
		return 1;
	}

	ctx->window	 = hwnd;
	ctx->dc		 = dc;
	ctx->gfx_surface = (gfx_surface_t){
		.api	= GFX_API_SOFTWARE,
		.handle = (u64)(uintptr_t)hwnd,
		.data	= ctx,
		.ops	= &surface_wsw_gfx_ops,
	};
	return 0;
}

static BITMAPINFO surface_wsw_bitmap_info(u16 width, u16 height)
{
	return (BITMAPINFO){
		.bmiHeader =
			{
				.biSize	     = (DWORD)sizeof(BITMAPINFOHEADER),
				.biWidth     = width,
				.biHeight    = -(LONG)height,
				.biPlanes    = 1,
				.biBitCount  = SURFACE_WSW_BIT_COUNT,
				.biCompression = BI_RGB,
				.biSizeImage = (DWORD)((size_t)width * height * 4),
			},
	};
}

static int surface_wsw_create_bitmap(surface_wsw_t *ctx, u16 width, u16 height)
{
	HDC memory_dc = ctx->wsw.CreateCompatibleDC(ctx->dc);
	if (memory_dc == NULL) {
		log_error("csurface", "wsw", NULL, "failed to create a Windows memory device context");
		return 1;
	}

	void *bitmap_pixels = NULL;
	BITMAPINFO info	    = surface_wsw_bitmap_info(width, height);
	HBITMAP bitmap	    = ctx->wsw.CreateDIBSection(ctx->dc, &info, DIB_RGB_COLORS, &bitmap_pixels, NULL, 0);
	if (bitmap == NULL || bitmap_pixels == NULL) {
		ctx->wsw.DeleteDC(memory_dc);
		log_error("csurface", "wsw", NULL, "failed to create a Windows DIB section");
		return 1;
	}

	HGDIOBJ old_bitmap = ctx->wsw.SelectObject(memory_dc, bitmap);
	if (old_bitmap == NULL) {
		ctx->wsw.DeleteObject(bitmap);
		ctx->wsw.DeleteDC(memory_dc);
		log_error("csurface", "wsw", NULL, "failed to select the Windows DIB section");
		return 1;
	}

	ctx->memory_dc     = memory_dc;
	ctx->bitmap	     = bitmap;
	ctx->old_bitmap    = old_bitmap;
	ctx->bitmap_pixels = bitmap_pixels;
	ctx->width	     = width;
	ctx->height	     = height;
	return 0;
}

static int surface_wsw_gfx_memory(gfx_surface_t *surface, gfx_surface_memory_t *memory)
{
	if (surface == NULL || surface->data == NULL || memory == NULL || memory->width == 0 || memory->height == 0) {
		return 1;
	}

	surface_wsw_t *ctx = surface->data;
	if (ctx->dc == NULL || ctx->window == NULL) {
		return 1;
	}

	if (ctx->bitmap != NULL && ctx->width == memory->width && ctx->height == memory->height) {
		memory->format = GFX_FORMAT_RGBA8;
		memory->data   = ctx->pixels;
		memory->stride = (size_t)ctx->width * 4;
		return 0;
	}

	surface_wsw_free_bitmap(ctx);

	size_t pixels_size = (size_t)memory->width * memory->height * 4;
	u8 *pixels	   = alloc_alloc(&ctx->alloc, pixels_size);
	if (pixels == NULL) {
		log_error("csurface", "wsw", NULL, "failed to allocate surface pixels");
		return 1;
	}
	mem_set(pixels, 0, pixels_size);

	ctx->pixels	 = pixels;
	ctx->pixels_size = pixels_size;
	if (surface_wsw_create_bitmap(ctx, memory->width, memory->height)) {
		alloc_free(&ctx->alloc, pixels, pixels_size);
		ctx->pixels	 = NULL;
		ctx->pixels_size = 0;
		return 1;
	}

	memory->format = GFX_FORMAT_RGBA8;
	memory->data   = ctx->pixels;
	memory->stride = (size_t)ctx->width * 4;
	return 0;
}

static void surface_wsw_convert(surface_wsw_t *ctx)
{
	for (u16 y = 0; y < ctx->height; y++) {
		const u8 *src = ctx->pixels + (size_t)y * ctx->width * 4;
		u8 *dst	      = ctx->bitmap_pixels + (size_t)y * ctx->width * 4;
		for (u16 x = 0; x < ctx->width; x++) {
			dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0];
			dst[3] = src[3];
			src += 4;
			dst += 4;
		}
	}
}

static int surface_wsw_gfx_present(gfx_surface_t *surface, gfx_present_mode_t present_mode)
{
	(void)present_mode;

	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_wsw_t *ctx = surface->data;
	if (ctx->dc == NULL || ctx->memory_dc == NULL || ctx->bitmap == NULL || ctx->pixels == NULL || ctx->bitmap_pixels == NULL) {
		return 1;
	}

	surface_wsw_convert(ctx);
	return ctx->wsw.BitBlt(ctx->dc, 0, 0, ctx->width, ctx->height, ctx->memory_dc, 0, 0, SRCCOPY) ? 0 : 1;
}

static const gfx_surface_ops_t surface_wsw_gfx_ops = {
	.present = surface_wsw_gfx_present,
	.memory	 = surface_wsw_gfx_memory,
};

static int surface_wsw_native(surface_t *srf, surface_native_t *native)
{
	if (srf == NULL || srf->data == NULL || native == NULL) {
		return 1;
	}

	surface_wsw_t *ctx = srf->data;
	if (ctx->window == NULL || ctx->dc == NULL) {
		return 1;
	}

	*native = (surface_native_t){
		.gfx_api     = GFX_API_SOFTWARE,
		.native_type = DISPLAY_NATIVE_WINDOWS,
		.display     = ctx->dc,
		.handle	     = (u64)(uintptr_t)ctx->window,
		.gfx_surface = &ctx->gfx_surface,
	};
	return 0;
}

static surface_driver_t surface_wsw = {
	.name		= "wsw",
	.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND,
	.compatible	= surface_wsw_compatible,
	.init		= surface_wsw_init,
	.free		= surface_wsw_free,
	.config_window	= surface_wsw_config_window,
	.bind		= surface_wsw_bind,
	.unbind		= surface_wsw_unbind,
	.native		= surface_wsw_native,
};

SURFACE_DRIVER(surface_wsw, &surface_wsw);

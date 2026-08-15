#include "surface_driver.h"
#include "surface_platform.h"

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
	BI_RGB		      = 0,
	DIB_RGB_COLORS	      = 0,
	SRCCOPY		      = 0x00CC0020,
	SURFACE_WSW_BIT_COUNT = 32,
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
	u8 *bitmap_pixels;
	u16 width;
	u16 height;
	gfx_surface_t gfx_surface;
} surface_wsw_t;

static int surface_wsw_load_symbol(surface_wsw_t *ctx, void *lib, void **sym, strv_t name)
{
	return surface_platform_load_symbol(ctx->proc, lib, sym, name, "wsw", "Win32");
}

#define LOAD_USER32(_ctx, _name) surface_wsw_load_symbol((_ctx), (_ctx)->user32, (void **)&(_ctx)->wsw._name, STRV(#_name))
#define LOAD_GDI32(_ctx, _name)	 surface_wsw_load_symbol((_ctx), (_ctx)->gdi32, (void **)&(_ctx)->wsw._name, STRV(#_name))

static void surface_wsw_unload(surface_wsw_t *ctx)
{
	surface_platform_library_t libraries[] = {
		{STRV("gdi32.dll"), &ctx->gdi32},
		{STRV("user32.dll"), &ctx->user32},
	};
	surface_platform_close_library_set(ctx->proc, libraries, sizeof(libraries) / sizeof(libraries[0]));
}

static int surface_wsw_load(surface_wsw_t *ctx, proc_t *proc)
{
	ctx->proc			       = proc;
	surface_platform_library_t libraries[] = {
		{STRV("user32.dll"), &ctx->user32},
		{STRV("gdi32.dll"), &ctx->gdi32},
	};
	if (surface_platform_load_libraries(ctx->proc, libraries, sizeof(libraries) / sizeof(libraries[0]), "wsw")) {
		return 1;
	}

	if (LOAD_USER32(ctx, GetDC) || LOAD_USER32(ctx, ReleaseDC) || LOAD_GDI32(ctx, CreateCompatibleDC) || LOAD_GDI32(ctx, DeleteDC) ||
	    LOAD_GDI32(ctx, CreateDIBSection) || LOAD_GDI32(ctx, SelectObject) || LOAD_GDI32(ctx, DeleteObject) ||
	    LOAD_GDI32(ctx, BitBlt)) {
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

static int surface_wsw_init(surface_backend_t *srf, const surface_backend_config_t *config)
{
	if (srf == NULL || config == NULL || config->display == NULL || config->display->proc == NULL) {
		return 1;
	}

	surface_wsw_t *ctx = surface_platform_alloc(srf, config, sizeof(*ctx), "wsw", 1);
	if (ctx == NULL) {
		return 1;
	}

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
	ctx->memory_dc	   = NULL;
	ctx->bitmap	   = NULL;
	ctx->old_bitmap	   = NULL;
	ctx->bitmap_pixels = NULL;
	ctx->width	   = 0;
	ctx->height	   = 0;
}

static int surface_wsw_unbind(surface_backend_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_wsw_t *ctx = srf->data;
	surface_wsw_free_bitmap(ctx);
	if (ctx->dc != NULL) {
		ctx->wsw.ReleaseDC(ctx->window, ctx->dc);
	}
	ctx->window	 = NULL;
	ctx->dc		 = NULL;
	ctx->gfx_surface = (gfx_surface_t){0};
	return 0;
}

static int surface_wsw_free(surface_backend_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_wsw_t *ctx = srf->data;
	surface_wsw_unbind(srf);
	surface_wsw_unload(ctx);
	surface_platform_free(srf, sizeof(*ctx));
	return 0;
}

static int surface_wsw_config_window(surface_backend_t *srf, window_config_t *config)
{
	if (srf == NULL || srf->data == NULL || config == NULL) {
		return 1;
	}

	display_native_t native = {0};
	if (surface_platform_display_native(srf, DISPLAY_NATIVE_WINDOWS, &native, "wsw", "Windows")) {
		return 1;
	}

	surface_platform_default_window_config(config);
	return 0;
}

static const gfx_surface_ops_t surface_wsw_gfx_ops;

static int surface_wsw_bind(surface_backend_t *srf, window_t *window)
{
	if (srf == NULL || srf->data == NULL || window == NULL) {
		return 1;
	}

	display_native_t native_display = {0};
	if (surface_platform_display_native(srf, DISPLAY_NATIVE_WINDOWS, &native_display, "wsw", "Windows")) {
		return 1;
	}

	window_native_t native_window = {0};
	if (surface_platform_window_native(window, DISPLAY_NATIVE_WINDOWS, &native_window, "wsw", "Windows")) {
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

	ctx->window = hwnd;
	ctx->dc	    = dc;
	surface_platform_gfx_surface(&ctx->gfx_surface, GFX_API_SOFTWARE, (u64)(uintptr_t)hwnd, ctx, &surface_wsw_gfx_ops);
	return 0;
}

static BITMAPINFO surface_wsw_bitmap_info(u16 width, u16 height)
{
	return (BITMAPINFO){
		.bmiHeader =
			{
				.biSize	       = (DWORD)sizeof(BITMAPINFOHEADER),
				.biWidth       = width,
				.biHeight      = -(LONG)height,
				.biPlanes      = 1,
				.biBitCount    = SURFACE_WSW_BIT_COUNT,
				.biCompression = BI_RGB,
				.biSizeImage   = (DWORD)((size_t)width * height * 4),
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

	ctx->memory_dc	   = memory_dc;
	ctx->bitmap	   = bitmap;
	ctx->old_bitmap	   = old_bitmap;
	ctx->bitmap_pixels = bitmap_pixels;
	ctx->width	   = width;
	ctx->height	   = height;
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
		memory->format = GFX_FORMAT_BGRA8_UNORM;
		memory->data   = ctx->bitmap_pixels;
		memory->stride = (size_t)ctx->width * 4;
		return 0;
	}

	surface_wsw_free_bitmap(ctx);

	if (surface_wsw_create_bitmap(ctx, memory->width, memory->height)) {
		return 1;
	}

	memory->format = GFX_FORMAT_BGRA8_UNORM;
	memory->data   = ctx->bitmap_pixels;
	memory->stride = (size_t)ctx->width * 4;
	return 0;
}

static int surface_wsw_gfx_present(gfx_surface_t *surface, gfx_present_mode_t present_mode)
{
	(void)present_mode;

	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_wsw_t *ctx = surface->data;
	if (ctx->dc == NULL || ctx->memory_dc == NULL || ctx->bitmap == NULL || ctx->bitmap_pixels == NULL) {
		return 1;
	}

	return ctx->wsw.BitBlt(ctx->dc, 0, 0, ctx->width, ctx->height, ctx->memory_dc, 0, 0, SRCCOPY) ? 0 : 1;
}

static const gfx_surface_ops_t surface_wsw_gfx_ops = {
	.present = surface_wsw_gfx_present,
	.memory	 = surface_wsw_gfx_memory,
};

static int surface_wsw_native(surface_backend_t *srf, surface_native_t *native)
{
	if (srf == NULL || srf->data == NULL || native == NULL) {
		return 1;
	}

	surface_wsw_t *ctx = srf->data;
	if (ctx->window == NULL || ctx->dc == NULL) {
		return 1;
	}

	surface_platform_native(
		native, GFX_API_SOFTWARE, DISPLAY_NATIVE_WINDOWS, ctx->dc, NULL, (u64)(uintptr_t)ctx->window, &ctx->gfx_surface);
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

#include "surface_driver.h"
#include "surface_platform.h"

#include "log.h"
#include "mem.h"

typedef void *HDC;
typedef void *HWND;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef int BOOL;
typedef BOOL (*wgl_swap_interval_ext_t)(int);

typedef struct PIXELFORMATDESCRIPTOR_s {
	WORD nSize;
	WORD nVersion;
	DWORD dwFlags;
	BYTE iPixelType;
	BYTE cColorBits;
	BYTE cRedBits;
	BYTE cRedShift;
	BYTE cGreenBits;
	BYTE cGreenShift;
	BYTE cBlueBits;
	BYTE cBlueShift;
	BYTE cAlphaBits;
	BYTE cAlphaShift;
	BYTE cAccumBits;
	BYTE cAccumRedBits;
	BYTE cAccumGreenBits;
	BYTE cAccumBlueBits;
	BYTE cAccumAlphaBits;
	BYTE cDepthBits;
	BYTE cStencilBits;
	BYTE cAuxBuffers;
	BYTE iLayerType;
	BYTE bReserved;
	DWORD dwLayerMask;
	DWORD dwVisibleMask;
	DWORD dwDamageMask;
} PIXELFORMATDESCRIPTOR;

enum {
	PFD_DOUBLEBUFFER   = 0x00000001,
	PFD_DRAW_TO_WINDOW = 0x00000004,
	PFD_SUPPORT_OPENGL = 0x00000020,
	PFD_TYPE_RGBA	   = 0,
	PFD_MAIN_PLANE	   = 0,
};

typedef struct wgl_s {
	HDC (*GetDC)(HWND);
	int (*ReleaseDC)(HWND, HDC);
	int (*ChoosePixelFormat)(HDC, const PIXELFORMATDESCRIPTOR *);
	BOOL (*SetPixelFormat)(HDC, int, const PIXELFORMATDESCRIPTOR *);
	int (*GetPixelFormat)(HDC);
	int (*DescribePixelFormat)(HDC, int, UINT, PIXELFORMATDESCRIPTOR *);
	void *(*GetProcAddress)(const char *);
	void *(*CreateContext)(HDC);
	BOOL (*DeleteContext)(void *);
	BOOL (*MakeCurrent)(HDC, void *);
	BOOL (*SwapBuffers)(HDC);
	wgl_swap_interval_ext_t SwapIntervalEXT;
} wgl_t;

typedef struct surface_wgl_s {
	proc_t *proc;
	void *user32;
	void *gdi32;
	void *opengl32;
	wgl_t wgl;
	HWND window;
	HDC dc;
	void *context;
	int pixel_format;
	int swap_interval_loaded;
	gfx_present_mode_t present_mode;
	gfx_surface_t gfx_surface;
} surface_wgl_t;

static int surface_wgl_load_symbol(surface_wgl_t *ctx, void *lib, void **sym, strv_t name)
{
	return surface_platform_load_symbol(ctx->proc, lib, sym, name, "wgl", "Win32");
}

#define LOAD_USER32(_ctx, _name) surface_wgl_load_symbol((_ctx), (_ctx)->user32, (void **)&(_ctx)->wgl._name, STRV(#_name))
#define LOAD_GDI32(_ctx, _name)	 surface_wgl_load_symbol((_ctx), (_ctx)->gdi32, (void **)&(_ctx)->wgl._name, STRV(#_name))

static void surface_wgl_unload(surface_wgl_t *ctx)
{
	surface_platform_library_t libraries[] = {
		{STRV("gdi32.dll"), &ctx->gdi32},
		{STRV("opengl32.dll"), &ctx->opengl32},
		{STRV("user32.dll"), &ctx->user32},
	};
	surface_platform_close_library_set(ctx->proc, libraries, sizeof(libraries) / sizeof(libraries[0]));
}

static int surface_wgl_load(surface_wgl_t *ctx, proc_t *proc)
{
	ctx->proc			       = proc;
	surface_platform_library_t libraries[] = {
		{STRV("user32.dll"), &ctx->user32},
		{STRV("gdi32.dll"), &ctx->gdi32},
		{STRV("opengl32.dll"), &ctx->opengl32},
	};
	if (surface_platform_load_libraries(ctx->proc, libraries, sizeof(libraries) / sizeof(libraries[0]), "wgl")) {
		return 1;
	}

	if (LOAD_USER32(ctx, GetDC) || LOAD_USER32(ctx, ReleaseDC) || LOAD_GDI32(ctx, ChoosePixelFormat) ||
	    LOAD_GDI32(ctx, SetPixelFormat) || LOAD_GDI32(ctx, GetPixelFormat) || LOAD_GDI32(ctx, DescribePixelFormat) ||
	    surface_wgl_load_symbol(ctx, ctx->gdi32, (void **)&ctx->wgl.SwapBuffers, STRV("SwapBuffers")) ||
	    surface_wgl_load_symbol(ctx, ctx->opengl32, (void **)&ctx->wgl.GetProcAddress, STRV("wglGetProcAddress")) ||
	    surface_wgl_load_symbol(ctx, ctx->opengl32, (void **)&ctx->wgl.CreateContext, STRV("wglCreateContext")) ||
	    surface_wgl_load_symbol(ctx, ctx->opengl32, (void **)&ctx->wgl.DeleteContext, STRV("wglDeleteContext")) ||
	    surface_wgl_load_symbol(ctx, ctx->opengl32, (void **)&ctx->wgl.MakeCurrent, STRV("wglMakeCurrent"))) {
		surface_wgl_unload(ctx);
		mem_set(&ctx->wgl, 0, sizeof(ctx->wgl));
		return 1;
	}

	return 0;
}

static int surface_wgl_compatible(const surface_info_t *info)
{
	return info != NULL && info->gfx_api == GFX_API_OPENGL && info->native_type == DISPLAY_NATIVE_WINDOWS;
}

static int surface_wgl_init(surface_backend_t *srf, const surface_backend_config_t *config)
{
	if (srf == NULL || config == NULL || config->display == NULL || config->display->proc == NULL) {
		return 1;
	}

	surface_wgl_t *ctx = surface_platform_alloc(srf, config, sizeof(*ctx), "wgl", 1);
	if (ctx == NULL) {
		return 1;
	}

	if (surface_wgl_load(ctx, config->display->proc)) {
		alloc_free(&srf->alloc, ctx, sizeof(*ctx));
		return 1;
	}

	srf->data = ctx;
	return 0;
}

static int surface_wgl_unbind(surface_backend_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_wgl_t *ctx = srf->data;
	if (ctx->context != NULL) {
		ctx->wgl.MakeCurrent(NULL, NULL);
		ctx->wgl.DeleteContext(ctx->context);
	}
	if (ctx->dc != NULL) {
		ctx->wgl.ReleaseDC(ctx->window, ctx->dc);
	}
	ctx->window		  = NULL;
	ctx->dc			  = NULL;
	ctx->context		  = NULL;
	ctx->pixel_format	  = 0;
	ctx->swap_interval_loaded = 0;
	ctx->wgl.SwapIntervalEXT  = NULL;
	ctx->present_mode	  = GFX_PRESENT_MODE_DEFAULT;
	ctx->gfx_surface	  = (gfx_surface_t){0};
	return 0;
}

static int surface_wgl_free(surface_backend_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_wgl_t *ctx = srf->data;
	surface_wgl_unbind(srf);
	surface_wgl_unload(ctx);
	surface_platform_free(srf, sizeof(*ctx));
	return 0;
}

static int surface_wgl_config_window(surface_backend_t *srf, window_config_t *config)
{
	if (srf == NULL || srf->data == NULL || config == NULL) {
		return 1;
	}

	display_native_t native = {0};
	if (surface_platform_display_native(srf, DISPLAY_NATIVE_WINDOWS, &native, "wgl", "Windows")) {
		return 1;
	}

	surface_platform_default_window_config(config);
	return 0;
}

static PIXELFORMATDESCRIPTOR surface_wgl_pixel_format_descriptor(void)
{
	return (PIXELFORMATDESCRIPTOR){
		.nSize	      = (WORD)sizeof(PIXELFORMATDESCRIPTOR),
		.nVersion     = 1,
		.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		.iPixelType   = PFD_TYPE_RGBA,
		.cColorBits   = 32,
		.cAlphaBits   = 8,
		.cDepthBits   = 24,
		.cStencilBits = 8,
		.iLayerType   = PFD_MAIN_PLANE,
	};
}

static int surface_wgl_pixel_format_supported(const PIXELFORMATDESCRIPTOR *format)
{
	DWORD flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	return format != NULL && format->iPixelType == PFD_TYPE_RGBA && (format->dwFlags & flags) == flags;
}

static int surface_wgl_configure_pixel_format(surface_wgl_t *ctx, HDC dc)
{
	PIXELFORMATDESCRIPTOR format = surface_wgl_pixel_format_descriptor();
	int pixel_format	     = ctx->wgl.GetPixelFormat(dc);
	if (pixel_format != 0) {
		PIXELFORMATDESCRIPTOR current = {0};
		if (!ctx->wgl.DescribePixelFormat(dc, pixel_format, (UINT)sizeof(current), &current) ||
		    !surface_wgl_pixel_format_supported(&current)) {
			log_error("csurface", "wgl", NULL, "current Windows pixel format does not support OpenGL");
			return 1;
		}
		ctx->pixel_format = pixel_format;
		return 0;
	}

	pixel_format = ctx->wgl.ChoosePixelFormat(dc, &format);
	if (pixel_format == 0) {
		log_error("csurface", "wgl", NULL, "no double-buffered RGBA WGL pixel format is available");
		return 1;
	}
	if (!ctx->wgl.SetPixelFormat(dc, pixel_format, &format)) {
		log_error("csurface", "wgl", NULL, "failed to set the WGL pixel format");
		return 1;
	}

	ctx->pixel_format = pixel_format;
	return 0;
}

static int surface_wgl_gfx_proc(gfx_surface_t *surface, strv_t name, void **proc)
{
	if (surface == NULL || surface->data == NULL || proc == NULL) {
		return 1;
	}

	surface_wgl_t *ctx = surface->data;
	*proc		   = ctx->wgl.GetProcAddress(name.data);
	return *proc == NULL;
}

static int surface_wgl_gfx_make_current(gfx_surface_t *surface)
{
	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_wgl_t *ctx = surface->data;
	return !ctx->wgl.MakeCurrent(ctx->dc, ctx->context);
}

static int surface_wgl_gfx_clear_current(gfx_surface_t *surface)
{
	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_wgl_t *ctx = surface->data;
	return !ctx->wgl.MakeCurrent(NULL, NULL);
}

static void surface_wgl_load_swap_interval(surface_wgl_t *ctx)
{
	if (ctx->swap_interval_loaded) {
		return;
	}

	union {
		void *data;
		wgl_swap_interval_ext_t proc;
	} sym;
	sym.data		  = ctx->wgl.GetProcAddress("wglSwapIntervalEXT");
	ctx->wgl.SwapIntervalEXT  = sym.proc;
	ctx->swap_interval_loaded = 1;
}

static int surface_wgl_gfx_present_mode(gfx_surface_t *surface, gfx_present_mode_t requested, gfx_present_mode_t *actual)
{
	if (surface == NULL || surface->data == NULL || actual == NULL) {
		return 1;
	}

	surface_wgl_t *ctx = surface->data;
	surface_wgl_load_swap_interval(ctx);
	switch (requested) {
	case GFX_PRESENT_MODE_DEFAULT:
		*actual = GFX_PRESENT_MODE_DEFAULT;
		return 0;
	case GFX_PRESENT_MODE_IMMEDIATE:
		*actual = ctx->wgl.SwapIntervalEXT != NULL ? GFX_PRESENT_MODE_IMMEDIATE : GFX_PRESENT_MODE_DEFAULT;
		return 0;
	case GFX_PRESENT_MODE_MAILBOX:
	case GFX_PRESENT_MODE_VSYNC:
		*actual = ctx->wgl.SwapIntervalEXT != NULL ? GFX_PRESENT_MODE_VSYNC : GFX_PRESENT_MODE_DEFAULT;
		return 0;
	default:
		return 1;
	}
}

static int surface_wgl_set_swap_interval(surface_wgl_t *ctx, gfx_present_mode_t present_mode)
{
	if (ctx->present_mode == present_mode || present_mode == GFX_PRESENT_MODE_DEFAULT) {
		return 0;
	}

	surface_wgl_load_swap_interval(ctx);
	if (ctx->wgl.SwapIntervalEXT == NULL) {
		return 1;
	}
	if (!ctx->wgl.SwapIntervalEXT(present_mode == GFX_PRESENT_MODE_IMMEDIATE ? 0 : 1)) {
		return 1;
	}
	ctx->present_mode = present_mode;
	return 0;
}

static int surface_wgl_gfx_present(gfx_surface_t *surface, gfx_present_mode_t present_mode)
{
	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_wgl_t *ctx = surface->data;
	if (surface_wgl_set_swap_interval(ctx, present_mode)) {
		return 1;
	}
	return !ctx->wgl.SwapBuffers(ctx->dc);
}

static const gfx_surface_ops_t surface_wgl_gfx_ops = {
	.proc	       = surface_wgl_gfx_proc,
	.make_current  = surface_wgl_gfx_make_current,
	.clear_current = surface_wgl_gfx_clear_current,
	.present_mode  = surface_wgl_gfx_present_mode,
	.present       = surface_wgl_gfx_present,
};

static int surface_wgl_bind(surface_backend_t *srf, window_t *window)
{
	if (srf == NULL || srf->data == NULL || window == NULL) {
		return 1;
	}

	display_native_t native_display = {0};
	if (surface_platform_display_native(srf, DISPLAY_NATIVE_WINDOWS, &native_display, "wgl", "Windows")) {
		return 1;
	}

	window_native_t native_window = {0};
	if (surface_platform_window_native(window, DISPLAY_NATIVE_WINDOWS, &native_window, "wgl", "Windows")) {
		return 1;
	}

	surface_wgl_t *ctx = srf->data;
	if (ctx->dc != NULL) {
		surface_wgl_unbind(srf);
	}

	HWND hwnd = native_window.window;
	HDC dc	  = ctx->wgl.GetDC(hwnd);
	if (dc == NULL) {
		log_error("csurface", "wgl", NULL, "failed to get a Windows device context");
		return 1;
	}

	ctx->window = hwnd;
	ctx->dc	    = dc;
	if (surface_wgl_configure_pixel_format(ctx, dc)) {
		surface_wgl_unbind(srf);
		return 1;
	}
	ctx->context = ctx->wgl.CreateContext(dc);
	if (ctx->context == NULL) {
		log_error("csurface", "wgl", NULL, "failed to create WGL context");
		surface_wgl_unbind(srf);
		return 1;
	}
	surface_platform_gfx_surface(&ctx->gfx_surface, GFX_API_OPENGL, (u64)(uintptr_t)hwnd, ctx, &surface_wgl_gfx_ops);

	return 0;
}

static int surface_wgl_native(surface_backend_t *srf, surface_native_t *native)
{
	if (srf == NULL || srf->data == NULL || native == NULL) {
		return 1;
	}

	surface_wgl_t *ctx = srf->data;
	if (ctx->window == NULL || ctx->dc == NULL || ctx->pixel_format == 0) {
		return 1;
	}

	surface_platform_native(native,
				GFX_API_OPENGL,
				DISPLAY_NATIVE_WINDOWS,
				ctx->dc,
				(void *)(uintptr_t)ctx->pixel_format,
				(u64)(uintptr_t)ctx->window,
				&ctx->gfx_surface);
	return 0;
}

static surface_driver_t surface_wgl = {
	.name		= "wgl",
	.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND,
	.compatible	= surface_wgl_compatible,
	.init		= surface_wgl_init,
	.free		= surface_wgl_free,
	.config_window	= surface_wgl_config_window,
	.bind		= surface_wgl_bind,
	.unbind		= surface_wgl_unbind,
	.native		= surface_wgl_native,
};

SURFACE_DRIVER(surface_wgl, &surface_wgl);

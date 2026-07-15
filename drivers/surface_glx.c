#include "surface_driver.h"

#include "log.h"
#include "mem.h"

typedef void Display;
typedef void Visual;
typedef unsigned long XID;
typedef unsigned long Window;
typedef unsigned long VisualID;
typedef int Bool;

typedef struct XVisualInfo_s {
	Visual *visual;
	VisualID visualid;
	int screen;
	int depth;
	int class;
	unsigned long red_mask;
	unsigned long green_mask;
	unsigned long blue_mask;
	int colormap_size;
	int bits_per_rgb;
} XVisualInfo;

typedef XID GLXDrawable;
typedef void *GLXContext;

enum {
	GL_TRUE		 = 1,
	GLX_RGBA	 = 4,
	GLX_DOUBLEBUFFER = 5,
};

typedef struct glx_s {
	Bool (*XQueryVersion)(Display *, int *, int *);
	XVisualInfo *(*XChooseVisual)(Display *, int, int *);
	GLXContext (*XCreateContext)(Display *, XVisualInfo *, GLXContext, Bool);
	void (*XDestroyContext)(Display *, GLXContext);
	Bool (*XMakeCurrent)(Display *, GLXDrawable, GLXContext);
	void (*XSwapBuffers)(Display *, GLXDrawable);
} glx_t;

typedef struct surface_glx_s {
	glx_t glx;
	display_t *cdisplay;
	Display *display;
	XVisualInfo *visual;
	GLXContext context;
	Window window;
} surface_glx_t;

static int surface_glx_load_symbol(gfx_t *gfx, void **sym, strv_t name)
{
	if (gfx_proc(gfx, name, sym)) {
		log_error("csurface", "glx", NULL, "failed to load GLX symbol: %.*s", name.len, name.data);
		return 1;
	}

	return 0;
}

#define LOAD_GLX(_gfx, _ctx, _name) surface_glx_load_symbol((_gfx), (void **)&(_ctx)->glx._name, STRV("gl" #_name))

static int surface_glx_load(surface_glx_t *ctx, gfx_t *gfx)
{
	if (LOAD_GLX(gfx, ctx, XQueryVersion) || LOAD_GLX(gfx, ctx, XChooseVisual) || LOAD_GLX(gfx, ctx, XCreateContext) ||
	    LOAD_GLX(gfx, ctx, XDestroyContext) || LOAD_GLX(gfx, ctx, XMakeCurrent) || LOAD_GLX(gfx, ctx, XSwapBuffers)) {
		mem_set(&ctx->glx, 0, sizeof(ctx->glx));
		return 1;
	}

	return 0;
}

static int surface_glx_compatible(const surface_info_t *info)
{
	return info != NULL && info->gfx_api == GFX_API_OPENGL && info->native_type == DISPLAY_NATIVE_X11;
}

static int surface_glx_init(surface_t *srf, const surface_config_t *config)
{
	if (srf == NULL || config == NULL || config->alloc.alloc == NULL) {
		return 1;
	}

	alloc_t alloc	   = config->alloc;
	surface_glx_t *ctx = alloc_alloc(&alloc, sizeof(*ctx));
	if (ctx == NULL) {
		log_error("csurface", "glx", NULL, "failed to allocate surface data");
		return 1;
	}
	mem_set(ctx, 0, sizeof(*ctx));

	if (surface_glx_load(ctx, config->gfx)) {
		alloc_free(&alloc, ctx, sizeof(*ctx));
		return 1;
	}

	srf->data = ctx;
	return 0;
}

static int surface_glx_unbind(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_glx_t *ctx = srf->data;
	if (ctx->display != NULL && ctx->context != NULL) {
		ctx->glx.XMakeCurrent(ctx->display, 0, NULL);
		ctx->glx.XDestroyContext(ctx->display, ctx->context);
		ctx->context = NULL;
	}
	ctx->window = 0;
	return 0;
}

static int surface_glx_free(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_glx_t *ctx = srf->data;
	surface_glx_unbind(srf);
	if (ctx->cdisplay != NULL && ctx->visual != NULL) {
		display_native_free(ctx->cdisplay, ctx->visual);
		ctx->visual = NULL;
	}

	alloc_free(&srf->config.alloc, ctx, sizeof(*ctx));
	srf->data = NULL;
	return 0;
}

static int surface_glx_config_window(surface_t *srf, window_config_t *config)
{
	if (srf == NULL || srf->data == NULL || config == NULL) {
		return 1;
	}

	surface_glx_t *ctx	= srf->data;
	display_native_t native = {0};
	if (display_native(srf->config.display, &native) || native.type != DISPLAY_NATIVE_X11 || native.display == NULL) {
		log_error("csurface", "glx", NULL, "X11 native display is unavailable");
		return 1;
	}

	int major = 0;
	int minor = 0;
	if (!ctx->glx.XQueryVersion(native.display, &major, &minor) || major != 1 || minor < 2) {
		log_error("csurface", "glx", NULL, "GLX 1.2 is unavailable");
		return 1;
	}

	if (ctx->cdisplay != NULL && ctx->visual != NULL) {
		display_native_free(ctx->cdisplay, ctx->visual);
		ctx->visual = NULL;
	}

	int attributes[] = {
		GLX_RGBA,
		GLX_DOUBLEBUFFER,
		0,
	};
	ctx->visual = ctx->glx.XChooseVisual(native.display, native.screen, attributes);
	if (ctx->visual == NULL) {
		log_error("csurface", "glx", NULL, "no double-buffered RGBA GLX visual is available");
		return 1;
	}

	ctx->cdisplay  = srf->config.display;
	ctx->display   = native.display;
	config->depth  = (u8)ctx->visual->depth;
	config->visual = (u32)ctx->visual->visualid;
	return 0;
}

static int surface_glx_bind(surface_t *srf, window_t *window)
{
	if (srf == NULL || srf->data == NULL || window == NULL) {
		return 1;
	}

	surface_glx_t *ctx = srf->data;
	if (ctx->display == NULL || ctx->visual == NULL) {
		return 1;
	}

	window_native_t native = {0};
	if (window_native(window, &native) || native.type != DISPLAY_NATIVE_X11 || native.window == NULL) {
		log_error("csurface", "glx", NULL, "X11 native window is unavailable");
		return 1;
	}

	if (ctx->context != NULL && surface_glx_unbind(srf)) {
		return 1;
	}

	ctx->window  = (Window)(uintptr_t)native.window;
	ctx->context = ctx->glx.XCreateContext(ctx->display, ctx->visual, NULL, GL_TRUE);
	if (ctx->context == NULL) {
		ctx->window = 0;
		log_error("csurface", "glx", NULL, "failed to create GLX context");
		return 1;
	}
	if (!ctx->glx.XMakeCurrent(ctx->display, (GLXDrawable)ctx->window, ctx->context)) {
		ctx->glx.XDestroyContext(ctx->display, ctx->context);
		ctx->context = NULL;
		ctx->window  = 0;
		log_error("csurface", "glx", NULL, "failed to make the GLX context current");
		return 1;
	}

	return 0;
}

static int surface_glx_present(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_glx_t *ctx = srf->data;
	if (ctx->display == NULL || ctx->context == NULL || ctx->window == 0) {
		return 1;
	}

	ctx->glx.XSwapBuffers(ctx->display, (GLXDrawable)ctx->window);
	return 0;
}

static surface_driver_t surface_glx = {
	.name	       = "glx",
	.compatible    = surface_glx_compatible,
	.init	       = surface_glx_init,
	.free	       = surface_glx_free,
	.config_window = surface_glx_config_window,
	.bind	       = surface_glx_bind,
	.unbind	       = surface_glx_unbind,
	.present       = surface_glx_present,
};

SURFACE_DRIVER(surface_glx, &surface_glx);

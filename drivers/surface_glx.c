#include "surface_driver.h"

#include "log.h"
#include "mem.h"

typedef void Display;
typedef void Visual;
typedef unsigned long Window;
typedef unsigned long VisualID;
typedef int Bool;
typedef void *GLXContext;

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

enum {
	GLX_RGBA	 = 4,
	GLX_DOUBLEBUFFER = 5,
};

typedef struct glx_s {
	Bool (*XQueryVersion)(Display *, int *, int *);
	XVisualInfo *(*XChooseVisual)(Display *, int, int *);
	GLXContext (*XCreateContext)(Display *, XVisualInfo *, GLXContext, Bool);
	void (*XDestroyContext)(Display *, GLXContext);
	Bool (*XMakeCurrent)(Display *, Window, GLXContext);
	void (*XSwapBuffers)(Display *, Window);
} glx_t;

typedef struct surface_glx_s {
	proc_t *proc;
	void *lib;
	glx_t glx;
	display_t *cdisplay;
	Display *display;
	XVisualInfo *visual;
	Window window;
	GLXContext context;
	gfx_surface_t gfx_surface;
} surface_glx_t;

static int surface_glx_load_symbol(surface_glx_t *ctx, void **sym, strv_t name)
{
	if (proc_dlsym(ctx->proc, ctx->lib, name, sym)) {
		log_error("csurface", "glx", NULL, "failed to load GLX symbol: %.*s", name.len, name.data);
		return 1;
	}

	return 0;
}

#define LOAD_GLX(_ctx, _name) surface_glx_load_symbol((_ctx), (void **)&(_ctx)->glx._name, STRV("gl" #_name))

static int surface_glx_load(surface_glx_t *ctx, proc_t *proc)
{
	ctx->proc = proc;
	if (proc_dlopen(ctx->proc, STRV("libGLX.so.0"), &ctx->lib) && proc_dlopen(ctx->proc, STRV("libGL.so.1"), &ctx->lib) &&
	    proc_dlopen(ctx->proc, STRV("libGL.so"), &ctx->lib)) {
		log_error("csurface", "glx", NULL, "failed to load GLX library");
		return 1;
	}
	if (LOAD_GLX(ctx, XQueryVersion) || LOAD_GLX(ctx, XChooseVisual) || LOAD_GLX(ctx, XCreateContext) ||
	    LOAD_GLX(ctx, XDestroyContext) || LOAD_GLX(ctx, XMakeCurrent) || LOAD_GLX(ctx, XSwapBuffers)) {
		mem_set(&ctx->glx, 0, sizeof(ctx->glx));
		proc_dlclose(ctx->proc, ctx->lib);
		ctx->lib = NULL;
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

	if (surface_glx_load(ctx, config->display->proc)) {
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
	if (ctx->context != NULL) {
		ctx->glx.XMakeCurrent(ctx->display, 0, NULL);
		ctx->glx.XDestroyContext(ctx->display, ctx->context);
	}
	ctx->window	 = 0;
	ctx->context	 = NULL;
	ctx->gfx_surface = (gfx_surface_t){0};
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
	if (ctx->lib != NULL) {
		proc_dlclose(ctx->proc, ctx->lib);
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

	ctx->cdisplay	   = srf->config.display;
	ctx->display	   = native.display;
	config->depth	   = (u8)ctx->visual->depth;
	config->visual	   = (u32)ctx->visual->visualid;
	config->background = WINDOW_BACKGROUND_NONE;
	return 0;
}

static const gfx_surface_ops_t surface_glx_gfx_ops;

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

	if (ctx->window != 0) {
		surface_glx_unbind(srf);
	}

	ctx->window = (Window)(uintptr_t)native.window;
	ctx->context = ctx->glx.XCreateContext(ctx->display, ctx->visual, NULL, 1);
	if (ctx->context == NULL) {
		ctx->window = 0;
		log_error("csurface", "glx", NULL, "failed to create GLX context");
		return 1;
	}
	ctx->gfx_surface = (gfx_surface_t){
		.api	= GFX_API_OPENGL,
		.handle = ctx->window,
		.data	= ctx,
		.ops	= &surface_glx_gfx_ops,
	};
	return 0;
}

static int surface_glx_gfx_proc(gfx_surface_t *surface, strv_t name, void **proc)
{
	if (surface == NULL || surface->data == NULL || proc == NULL) {
		return 1;
	}

	surface_glx_t *ctx = surface->data;
	return proc_dlsym(ctx->proc, ctx->lib, name, proc);
}

static int surface_glx_gfx_make_current(gfx_surface_t *surface)
{
	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_glx_t *ctx = surface->data;
	return !ctx->glx.XMakeCurrent(ctx->display, ctx->window, ctx->context);
}

static int surface_glx_gfx_clear_current(gfx_surface_t *surface)
{
	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_glx_t *ctx = surface->data;
	return !ctx->glx.XMakeCurrent(ctx->display, 0, NULL);
}

static int surface_glx_gfx_present(gfx_surface_t *surface)
{
	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_glx_t *ctx = surface->data;
	ctx->glx.XSwapBuffers(ctx->display, ctx->window);
	return 0;
}

static const gfx_surface_ops_t surface_glx_gfx_ops = {
	.proc	       = surface_glx_gfx_proc,
	.make_current  = surface_glx_gfx_make_current,
	.clear_current = surface_glx_gfx_clear_current,
	.present       = surface_glx_gfx_present,
};

static int surface_glx_native(surface_t *srf, surface_native_t *native)
{
	if (srf == NULL || srf->data == NULL || native == NULL) {
		return 1;
	}

	surface_glx_t *ctx = srf->data;
	if (ctx->display == NULL || ctx->visual == NULL || ctx->window == 0) {
		return 1;
	}

	*native = (surface_native_t){
		.gfx_api     = GFX_API_OPENGL,
		.native_type = DISPLAY_NATIVE_X11,
		.display     = ctx->display,
		.visual	     = ctx->visual,
		.handle	     = ctx->window,
		.gfx_surface = &ctx->gfx_surface,
	};
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
	.native	       = surface_glx_native,
};

SURFACE_DRIVER(surface_glx, &surface_glx);

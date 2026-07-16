#include "test.h"

#include "display_driver.h"
#include "gfx_driver.h"
#include "log.h"
#include "surface_driver.h"

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

typedef void (*t_surface_glx_symbol_t)(void);

static int t_glx_query_version_calls;
static int t_glx_choose_visual_calls;
static int t_glx_create_context_calls;
static int t_glx_destroy_context_calls;
static int t_glx_make_current_calls;
static int t_glx_swap_buffers_calls;
static int t_display_native_free_calls;
static Display *t_glx_display;
static int t_glx_screen;
static GLXDrawable t_glx_drawable;
static GLXContext t_glx_context;
static Bool t_glx_query_version_ret = 1;
static int t_glx_major		    = 1;
static int t_glx_minor		    = 2;
static XVisualInfo *t_glx_choose_visual_ret;
static GLXContext t_glx_create_context_ret = (GLXContext)0x5555;
static Bool t_glx_make_current_ret	   = 1;
static int t_display_native_ret;
static display_native_type_t t_display_native_type = DISPLAY_NATIVE_X11;
static void *t_display_native_display		   = (void *)0x1234;
static int t_window_native_ret;
static display_native_type_t t_window_native_type = DISPLAY_NATIVE_X11;
static void *t_window_native_window		  = (void *)(uintptr_t)0x4321;
static XVisualInfo t_glx_visual			  = {
	.visualid = 0x12345678,
	.depth	  = 24,
};

static void *t_surface_glx_symbol(t_surface_glx_symbol_t fn)
{
	union {
		t_surface_glx_symbol_t fn;
		void *ptr;
	} symbol = {.fn = fn};

	return symbol.ptr;
}

static Bool t_glXQueryVersion(Display *display, int *major, int *minor)
{
	t_glx_query_version_calls++;
	t_glx_display = display;
	*major	      = t_glx_major;
	*minor	      = t_glx_minor;
	return t_glx_query_version_ret;
}

static XVisualInfo *t_glXChooseVisual(Display *display, int screen, int *attributes)
{
	(void)attributes;
	t_glx_choose_visual_calls++;
	t_glx_display = display;
	t_glx_screen  = screen;
	return t_glx_choose_visual_ret;
}

static GLXContext t_glXCreateContext(Display *display, XVisualInfo *visual, GLXContext share, Bool direct)
{
	(void)visual;
	(void)share;
	(void)direct;
	t_glx_create_context_calls++;
	t_glx_display = display;
	return t_glx_create_context_ret;
}

static void t_glXDestroyContext(Display *display, GLXContext context)
{
	t_glx_destroy_context_calls++;
	t_glx_display = display;
	t_glx_context = context;
}

static Bool t_glXMakeCurrent(Display *display, GLXDrawable drawable, GLXContext context)
{
	t_glx_make_current_calls++;
	t_glx_display  = display;
	t_glx_drawable = drawable;
	t_glx_context  = context;
	return t_glx_make_current_ret;
}

static void t_glXSwapBuffers(Display *display, GLXDrawable drawable)
{
	t_glx_swap_buffers_calls++;
	t_glx_display  = display;
	t_glx_drawable = drawable;
}

static int t_surface_glx_gfx_proc(gfx_t *gfx, strv_t name, void **sym)
{
	void *lib = NULL;
	if (proc_dlopen(gfx->data, STRV("libGL.so.1"), &lib)) {
		return 1;
	}

	return proc_dlsym(gfx->data, lib, name, sym);
}

static gfx_driver_t t_surface_glx_gfx_driver = {
	.name = "test",
	.api  = GFX_API_OPENGL,
	.proc = t_surface_glx_gfx_proc,
};

static int t_surface_glx_display_native(display_t *display, display_native_t *native)
{
	(void)display;
	if (t_display_native_ret) {
		return t_display_native_ret;
	}
	*native = (display_native_t){
		.type	 = t_display_native_type,
		.display = t_display_native_display,
		.screen	 = 7,
	};
	return 0;
}

static int t_surface_glx_display_native_free(display_t *display, void *data)
{
	(void)display;
	(void)data;
	t_display_native_free_calls++;
	return 0;
}

static int t_surface_glx_window_native(window_t *window, window_native_t *native)
{
	(void)window;
	if (t_window_native_ret) {
		return t_window_native_ret;
	}
	*native = (window_native_t){
		.type	= t_window_native_type,
		.window = t_window_native_window,
	};
	return 0;
}

static display_driver_t t_surface_glx_display_driver = {
	.name	       = "test",
	.native	       = t_surface_glx_display_native,
	.native_free   = t_surface_glx_display_native_free,
	.window_native = t_surface_glx_window_native,
};

static surface_driver_t *t_surface_glx_driver(void)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type == SURFACE_DRIVER_TYPE) {
			surface_driver_t *drv = i->data;
			if (strv_eq(strv_cstr(drv->name), STRV("glx"))) {
				return drv;
			}
		}
	}

	return NULL;
}

static void t_surface_glx_reset(void)
{
	t_glx_query_version_calls   = 0;
	t_glx_choose_visual_calls   = 0;
	t_glx_create_context_calls  = 0;
	t_glx_destroy_context_calls = 0;
	t_glx_make_current_calls    = 0;
	t_glx_swap_buffers_calls    = 0;
	t_display_native_free_calls = 0;
	t_glx_display		    = NULL;
	t_glx_screen		    = 0;
	t_glx_drawable		    = 0;
	t_glx_context		    = NULL;
	t_glx_query_version_ret	    = 1;
	t_glx_major		    = 1;
	t_glx_minor		    = 2;
	t_glx_choose_visual_ret	    = &t_glx_visual;
	t_glx_create_context_ret    = (GLXContext)0x5555;
	t_glx_make_current_ret	    = 1;
	t_display_native_ret	    = 0;
	t_display_native_type	    = DISPLAY_NATIVE_X11;
	t_display_native_display    = (void *)0x1234;
	t_window_native_ret	    = 0;
	t_window_native_type	    = DISPLAY_NATIVE_X11;
	t_window_native_window	    = (void *)(uintptr_t)0x4321;
}

static void t_surface_glx_symbols(proc_t *proc)
{
	proc_setdlsym(proc, STRV("libGL.so.1"), STRV("glXQueryVersion"), t_surface_glx_symbol((t_surface_glx_symbol_t)t_glXQueryVersion));
	proc_setdlsym(proc, STRV("libGL.so.1"), STRV("glXChooseVisual"), t_surface_glx_symbol((t_surface_glx_symbol_t)t_glXChooseVisual));
	proc_setdlsym(proc, STRV("libGL.so.1"), STRV("glXCreateContext"), t_surface_glx_symbol((t_surface_glx_symbol_t)t_glXCreateContext));
	proc_setdlsym(
		proc, STRV("libGL.so.1"), STRV("glXDestroyContext"), t_surface_glx_symbol((t_surface_glx_symbol_t)t_glXDestroyContext));
	proc_setdlsym(proc, STRV("libGL.so.1"), STRV("glXMakeCurrent"), t_surface_glx_symbol((t_surface_glx_symbol_t)t_glXMakeCurrent));
	proc_setdlsym(proc, STRV("libGL.so.1"), STRV("glXSwapBuffers"), t_surface_glx_symbol((t_surface_glx_symbol_t)t_glXSwapBuffers));
}

static void *t_surface_glx_alloc_fail(alloc_t *alloc, size_t size)
{
	(void)alloc;
	(void)size;
	return NULL;
}

static int t_surface_glx_open(proc_t *proc, gfx_t *gfx, display_t *display, surface_t *surface)
{
	proc_init(proc, 0, 1, ALLOC_STD);
	t_surface_glx_symbols(proc);
	*gfx = (gfx_t){
		.drv  = &t_surface_glx_gfx_driver,
		.data = proc,
	};
	*display = (display_t){
		.drv = &t_surface_glx_display_driver,
	};

	return surface_init(surface,
			    &(surface_config_t){
				    .display = display,
				    .gfx     = gfx,
				    .alloc   = ALLOC_STD,
			    }) == NULL;
}

static int t_surface_glx_open_driver(proc_t *proc, gfx_t *gfx, display_t *display, surface_t *surface)
{
	proc_init(proc, 0, 1, ALLOC_STD);
	t_surface_glx_symbols(proc);
	*gfx = (gfx_t){
		.drv  = &t_surface_glx_gfx_driver,
		.data = proc,
	};
	*display = (display_t){
		.drv = &t_surface_glx_display_driver,
	};

	return surface_init_driver(surface,
				   t_surface_glx_driver(),
				   &(surface_config_t){
					   .display = display,
					   .gfx	    = gfx,
					   .alloc   = ALLOC_STD,
				   }) == NULL;
}

static void t_surface_glx_close(proc_t *proc, surface_t *surface)
{
	surface_free(surface);
	proc_free(proc);
}

TEST(surface_glx_driver_is_registered)
{
	START;

	EXPECT_NE(t_surface_glx_driver(), NULL);

	END;
}

TEST(surface_glx_init_rejects_non_opengl)
{
	START;

	t_surface_glx_reset();
	proc_t proc = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);
	t_surface_glx_symbols(&proc);
	gfx_driver_t drv = t_surface_glx_gfx_driver;
	drv.api		 = -1;
	gfx_t gfx	 = {
		.drv  = &drv,
		.data = &proc,
	};
	display_t display = {
		.drv = &t_surface_glx_display_driver,
	};
	surface_t surface = {0};

	EXPECT_EQ(surface_init(&surface,
			       &(surface_config_t){
				       .display = &display,
				       .gfx	= &gfx,
				       .alloc	= ALLOC_STD,
			       }),
		  NULL);

	proc_free(&proc);
	END;
}

TEST(surface_glx_init_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_glx_driver();
	EXPECT_NE(drv, NULL);

	EXPECT_EQ(drv->init(NULL, &(surface_config_t){.alloc = ALLOC_STD}), 1);

	END;
}

TEST(surface_glx_init_alloc_failure)
{
	START;

	t_surface_glx_reset();
	proc_t proc = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);
	t_surface_glx_symbols(&proc);
	gfx_t gfx = {
		.drv  = &t_surface_glx_gfx_driver,
		.data = &proc,
	};
	display_t display = {
		.drv = &t_surface_glx_display_driver,
	};
	surface_t surface = {0};

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_init_driver(&surface,
				      t_surface_glx_driver(),
				      &(surface_config_t){
					      .display = &display,
					      .gfx     = &gfx,
					      .alloc   = {.alloc = t_surface_glx_alloc_fail},
				      }),
		  NULL);
	log_set_quiet(0, 0);

	proc_free(&proc);
	END;
}

TEST(surface_glx_init_missing_symbol)
{
	START;

	t_surface_glx_reset();
	proc_t proc = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);
	gfx_t gfx = {
		.drv  = &t_surface_glx_gfx_driver,
		.data = &proc,
	};
	display_t display = {
		.drv = &t_surface_glx_display_driver,
	};
	surface_t surface = {0};

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_init_driver(&surface,
				      t_surface_glx_driver(),
				      &(surface_config_t){
					      .display = &display,
					      .gfx     = &gfx,
					      .alloc   = ALLOC_STD,
				      }),
		  NULL);
	log_set_quiet(0, 0);

	proc_free(&proc);
	END;
}

TEST(surface_glx_unbind_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_glx_driver();
	EXPECT_NE(drv, NULL);

	EXPECT_EQ(drv->unbind(NULL), 1);

	END;
}

TEST(surface_glx_free_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_glx_driver();
	EXPECT_NE(drv, NULL);

	EXPECT_EQ(drv->free(NULL), 1);

	END;
}

TEST(surface_glx_config_window_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_glx_driver();
	EXPECT_NE(drv, NULL);
	window_config_t config = {0};

	EXPECT_EQ(drv->config_window(NULL, &config), 1);

	END;
}

TEST(surface_glx_config_window_native_unavailable)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open_driver(&proc, &gfx, &display, &surface), 0);
	t_display_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_config_window(&surface, &config), 1);
	log_set_quiet(0, 0);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_config_window_version_unavailable)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open_driver(&proc, &gfx, &display, &surface), 0);
	t_glx_query_version_ret = 0;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_config_window(&surface, &config), 1);
	log_set_quiet(0, 0);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_config_window_replaces_visual)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);
	surface_config_window(&surface, &config);

	EXPECT_EQ(surface_config_window(&surface, &config), 0);
	EXPECT_EQ(t_display_native_free_calls, 1);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_config_window_no_visual)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open_driver(&proc, &gfx, &display, &surface), 0);
	t_glx_choose_visual_ret = NULL;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_config_window(&surface, &config), 1);
	log_set_quiet(0, 0);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_config_window_sets_depth)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);

	surface_config_window(&surface, &config);

	EXPECT_EQ(config.depth, 24);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_config_window_sets_visual)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);

	surface_config_window(&surface, &config);

	EXPECT_EQ(config.visual, 0x12345678);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_config_window_queries_version)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);

	surface_config_window(&surface, &config);

	EXPECT_EQ(t_glx_query_version_calls, 1);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_bind_sets_native_handle)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_t window	       = {.display = &display};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);
	surface_config_window(&surface, &config);

	surface_bind(&surface, &window);
	surface_native_t native = {0};
	surface_native(&surface, &native);

	EXPECT_EQ(native.handle, (u64)(uintptr_t)t_window_native_window);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_bind_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_glx_driver();
	EXPECT_NE(drv, NULL);
	window_t window = {0};

	EXPECT_EQ(drv->bind(NULL, &window), 1);

	END;
}

TEST(surface_glx_bind_without_window_config)
{
	START;

	t_surface_glx_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_bind_native_window_unavailable)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_t window	       = {.display = &display};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);
	surface_config_window(&surface, &config);
	t_window_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_bind_replaces_window)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_t window	       = {.display = &display};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);
	surface_config_window(&surface, &config);
	surface_bind(&surface, &window);
	t_window_native_window = (void *)(uintptr_t)0x5678;

	EXPECT_EQ(surface_bind(&surface, &window), 0);
	surface_native_t native = {0};
	surface_native(&surface, &native);
	EXPECT_EQ(native.handle, 0x5678u);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_native_returns_display)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_t window	       = {.display = &display};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);
	surface_config_window(&surface, &config);
	surface_bind(&surface, &window);
	surface_native_t native = {0};

	EXPECT_EQ(surface_native(&surface, &native), 0);
	EXPECT_EQ(native.display, t_display_native_display);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_native_returns_visual)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_t window	       = {.display = &display};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);
	surface_config_window(&surface, &config);
	surface_bind(&surface, &window);
	surface_native_t native = {0};

	EXPECT_EQ(surface_native(&surface, &native), 0);
	EXPECT_EQ(native.visual, &t_glx_visual);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_native_without_bind)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_t window	       = {.display = &display};
	window_config_t config = {0};
	(void)window;
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);
	surface_config_window(&surface, &config);
	surface_native_t native = {0};

	EXPECT_EQ(surface_native(&surface, &native), 1);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_native_null_native)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);

	EXPECT_EQ(surface.drv->native(&surface, NULL), 1);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_unbind_clears_native_handle)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_t window	       = {.display = &display};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);
	surface_config_window(&surface, &config);
	surface_bind(&surface, &window);

	surface_unbind(&surface);
	surface_native_t native = {0};

	EXPECT_EQ(surface_native(&surface, &native), 1);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_free_releases_visual)
{
	START;

	t_surface_glx_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_glx_open(&proc, &gfx, &display, &surface), 0);
	surface_config_window(&surface, &config);

	surface_free(&surface);

	EXPECT_EQ(t_display_native_free_calls, 1);

	proc_free(&proc);
	END;
}

STEST(surface_glx)
{
	SSTART;

	RUN(surface_glx_driver_is_registered);
	RUN(surface_glx_init_rejects_non_opengl);
	RUN(surface_glx_init_null_surface);
	RUN(surface_glx_init_alloc_failure);
	RUN(surface_glx_init_missing_symbol);
	RUN(surface_glx_unbind_null_surface);
	RUN(surface_glx_free_null_surface);
	RUN(surface_glx_config_window_null_surface);
	RUN(surface_glx_config_window_native_unavailable);
	RUN(surface_glx_config_window_version_unavailable);
	RUN(surface_glx_config_window_replaces_visual);
	RUN(surface_glx_config_window_no_visual);
	RUN(surface_glx_config_window_sets_depth);
	RUN(surface_glx_config_window_sets_visual);
	RUN(surface_glx_config_window_queries_version);
	RUN(surface_glx_bind_sets_native_handle);
	RUN(surface_glx_bind_null_surface);
	RUN(surface_glx_bind_without_window_config);
	RUN(surface_glx_bind_native_window_unavailable);
	RUN(surface_glx_bind_replaces_window);
	RUN(surface_glx_native_returns_display);
	RUN(surface_glx_native_returns_visual);
	RUN(surface_glx_native_without_bind);
	RUN(surface_glx_native_null_native);
	RUN(surface_glx_unbind_clears_native_handle);
	RUN(surface_glx_free_releases_visual);

	SEND;
}

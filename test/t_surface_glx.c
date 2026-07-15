#include "test.h"

#include "display_driver.h"
#include "gfx_driver.h"
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
static XVisualInfo t_glx_visual = {
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
	*major	      = 1;
	*minor	      = 2;
	return 1;
}

static XVisualInfo *t_glXChooseVisual(Display *display, int screen, int *attributes)
{
	(void)attributes;
	t_glx_choose_visual_calls++;
	t_glx_display = display;
	t_glx_screen  = screen;
	return &t_glx_visual;
}

static GLXContext t_glXCreateContext(Display *display, XVisualInfo *visual, GLXContext share, Bool direct)
{
	(void)visual;
	(void)share;
	(void)direct;
	t_glx_create_context_calls++;
	t_glx_display = display;
	return (GLXContext)0x5555;
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
	return 1;
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
	*native = (display_native_t){
		.type	 = DISPLAY_NATIVE_X11,
		.display = (void *)0x1234,
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
	*native = (window_native_t){
		.type	= DISPLAY_NATIVE_X11,
		.window = (void *)(uintptr_t)0x4321,
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

TEST(surface_glx_bind_creates_context)
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

	EXPECT_EQ(t_glx_create_context_calls, 1);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_bind_makes_current)
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

	EXPECT_EQ(t_glx_make_current_calls, 1);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_present_swaps_buffers)
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

	EXPECT_EQ(surface_present(&surface), 0);
	EXPECT_EQ(t_glx_swap_buffers_calls, 1);

	t_surface_glx_close(&proc, &surface);
	END;
}

TEST(surface_glx_unbind_destroys_context)
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

	EXPECT_EQ(t_glx_destroy_context_calls, 1);

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
	RUN(surface_glx_config_window_sets_depth);
	RUN(surface_glx_config_window_sets_visual);
	RUN(surface_glx_config_window_queries_version);
	RUN(surface_glx_bind_creates_context);
	RUN(surface_glx_bind_makes_current);
	RUN(surface_glx_present_swaps_buffers);
	RUN(surface_glx_unbind_destroys_context);
	RUN(surface_glx_free_releases_visual);

	SEND;
}

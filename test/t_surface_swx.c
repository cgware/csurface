#include "log.h"
#include "surface_driver.h"

#include "display_driver.h"
#include "gfx_driver.h"
#include "mem.h"
#include "test.h"

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

typedef void (*t_surface_swx_symbol_t)(void);

static int t_default_visual_calls;
static int t_default_depth_calls;
static int t_default_gc_calls;
static int t_visual_id_from_visual_calls;
static int t_create_image_calls;
static int t_destroy_image_calls;
static int t_put_image_calls;
static int t_flush_calls;
static int t_display_native_ret;
static int t_window_native_ret;
static display_native_type_t t_display_native_type;
static display_native_type_t t_window_native_type;
static Display *t_display_native_display;
static Window t_window_native_window;
static int t_display_native_screen;
static Visual *t_default_visual_ret;
static int t_default_depth_ret;
static GC t_default_gc_ret;
static VisualID t_visual_id_ret;
static XImage *t_create_image_ret;
static int t_create_image_null;
static int t_create_image_bad_bits;
static int t_create_image_bad_stride;
static int t_flush_ret;
static int t_alloc_calls;
static int t_alloc_fail_at;
static Window t_put_image_window;
static unsigned int t_put_image_width;
static unsigned int t_put_image_height;
static Visual t_visual = {
	.visualid   = 0x12345678,
	.red_mask   = 0x00FF0000,
	.green_mask = 0x0000FF00,
	.blue_mask  = 0x000000FF,
};
static XImage t_image;
static proc_t t_proc;
static display_t t_display;
static gfx_t t_gfx;
static window_t t_window;

static void *t_surface_swx_symbol(t_surface_swx_symbol_t fn)
{
	union {
		t_surface_swx_symbol_t fn;
		void *ptr;
	} symbol = {.fn = fn};

	return symbol.ptr;
}

static void *t_surface_swx_alloc(alloc_t *alloc, size_t size)
{
	(void)alloc;
	t_alloc_calls++;
	if (t_alloc_fail_at == t_alloc_calls) {
		return NULL;
	}

	return alloc_alloc_std(NULL, size);
}

static Visual *t_XDefaultVisual(Display *display, int screen)
{
	(void)display;
	(void)screen;
	t_default_visual_calls++;
	return t_default_visual_ret;
}

static int t_XDefaultDepth(Display *display, int screen)
{
	(void)display;
	(void)screen;
	t_default_depth_calls++;
	return t_default_depth_ret;
}

static GC t_XDefaultGC(Display *display, int screen)
{
	(void)display;
	(void)screen;
	t_default_gc_calls++;
	return t_default_gc_ret;
}

static VisualID t_XVisualIDFromVisual(Visual *visual)
{
	(void)visual;
	t_visual_id_from_visual_calls++;
	return t_visual_id_ret;
}

static XImage *t_XCreateImage(Display *display, Visual *visual, unsigned int depth, int format, int offset, char *data, unsigned int width,
			      unsigned int height, int bitmap_pad, int bytes_per_line)
{
	(void)display;
	(void)visual;
	(void)depth;
	(void)format;
	(void)offset;
	(void)data;
	(void)bitmap_pad;
	(void)bytes_per_line;
	t_create_image_calls++;
	if (t_create_image_null) {
		return NULL;
	}

	t_image.width	       = (int)width;
	t_image.height	       = (int)height;
	t_image.bits_per_pixel = t_create_image_bad_bits ? 0 : t_image.bits_per_pixel;
	t_image.bytes_per_line = t_create_image_bad_stride ? 0 : t_image.bytes_per_line;
	return t_create_image_ret;
}

static int t_XDestroyImage(XImage *image)
{
	(void)image;
	t_destroy_image_calls++;
	return 0;
}

static int t_XPutImage(Display *display, Window window, GC gc, XImage *image, int src_x, int src_y, int dst_x, int dst_y,
		       unsigned int width, unsigned int height)
{
	(void)display;
	(void)gc;
	(void)image;
	(void)src_x;
	(void)src_y;
	(void)dst_x;
	(void)dst_y;
	t_put_image_calls++;
	t_put_image_window = window;
	t_put_image_width  = width;
	t_put_image_height = height;
	return 0;
}

static int t_XFlush(Display *display)
{
	(void)display;
	t_flush_calls++;
	return t_flush_ret;
}

static int t_surface_swx_display_native(display_t *display, display_native_t *native)
{
	(void)display;
	if (t_display_native_ret) {
		return t_display_native_ret;
	}
	*native = (display_native_t){
		.type	 = t_display_native_type,
		.display = t_display_native_display,
		.screen	 = t_display_native_screen,
	};
	return 0;
}

static int t_surface_swx_window_native(window_t *window, window_native_t *native)
{
	(void)window;
	if (t_window_native_ret) {
		return t_window_native_ret;
	}
	*native = (window_native_t){
		.type	= t_window_native_type,
		.window = (void *)(uintptr_t)t_window_native_window,
	};
	return 0;
}

static display_driver_t t_surface_swx_display_driver = {
	.name	       = "test",
	.native	       = t_surface_swx_display_native,
	.window_native = t_surface_swx_window_native,
};

static gfx_driver_t t_surface_swx_gfx_driver = {
	.name = "test",
	.api  = GFX_API_SOFTWARE,
};

static surface_driver_t *t_surface_swx_driver(void)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type == SURFACE_DRIVER_TYPE) {
			surface_driver_t *drv = i->data;
			if (strv_eq(strv_cstr(drv->name), STRV("swx"))) {
				return drv;
			}
		}
	}

	return NULL;
}

static void t_surface_swx_reset(void)
{
	t_default_visual_calls	      = 0;
	t_default_depth_calls	      = 0;
	t_default_gc_calls	      = 0;
	t_visual_id_from_visual_calls = 0;
	t_create_image_calls	      = 0;
	t_destroy_image_calls	      = 0;
	t_put_image_calls	      = 0;
	t_flush_calls		      = 0;
	t_display_native_ret	      = 0;
	t_window_native_ret	      = 0;
	t_display_native_type	      = DISPLAY_NATIVE_X11;
	t_window_native_type	      = DISPLAY_NATIVE_X11;
	t_display_native_display      = (Display *)0x1234;
	t_window_native_window	      = 0x5678;
	t_display_native_screen	      = 7;
	t_default_visual_ret	      = &t_visual;
	t_default_depth_ret	      = 24;
	t_default_gc_ret	      = (GC)0x4321;
	t_visual_id_ret		      = 0x87654321;
	t_create_image_ret	      = &t_image;
	t_create_image_null	      = 0;
	t_create_image_bad_bits	      = 0;
	t_create_image_bad_stride     = 0;
	t_flush_ret		      = 1;
	t_alloc_calls		      = 0;
	t_alloc_fail_at		      = 0;
	t_put_image_window	      = 0;
	t_put_image_width	      = 0;
	t_put_image_height	      = 0;

	t_image = (XImage){
		.bits_per_pixel = 32,
		.bytes_per_line = 8,
		.red_mask	= 0x00FF0000,
		.green_mask	= 0x0000FF00,
		.blue_mask	= 0x000000FF,
	};
	t_proc	  = (proc_t){0};
	t_display = (display_t){0};
	t_gfx	  = (gfx_t){0};
	t_window  = (window_t){0};
}

static void t_surface_swx_symbols(proc_t *proc)
{
	proc_setdlsym(proc, STRV("libX11.so.6"), STRV("XDefaultVisual"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XDefaultVisual));
	proc_setdlsym(proc, STRV("libX11.so.6"), STRV("XDefaultDepth"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XDefaultDepth));
	proc_setdlsym(proc, STRV("libX11.so.6"), STRV("XDefaultGC"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XDefaultGC));
	proc_setdlsym(proc,
		      STRV("libX11.so.6"),
		      STRV("XVisualIDFromVisual"),
		      t_surface_swx_symbol((t_surface_swx_symbol_t)t_XVisualIDFromVisual));
	proc_setdlsym(proc, STRV("libX11.so.6"), STRV("XCreateImage"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XCreateImage));
	proc_setdlsym(proc, STRV("libX11.so.6"), STRV("XDestroyImage"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XDestroyImage));
	proc_setdlsym(proc, STRV("libX11.so.6"), STRV("XPutImage"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XPutImage));
	proc_setdlsym(proc, STRV("libX11.so.6"), STRV("XFlush"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XFlush));
}

static void t_surface_swx_symbols_fallback(proc_t *proc)
{
	proc_setdlsym(proc, STRV("libX11.so"), STRV("XDefaultVisual"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XDefaultVisual));
	proc_setdlsym(proc, STRV("libX11.so"), STRV("XDefaultDepth"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XDefaultDepth));
	proc_setdlsym(proc, STRV("libX11.so"), STRV("XDefaultGC"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XDefaultGC));
	proc_setdlsym(
		proc, STRV("libX11.so"), STRV("XVisualIDFromVisual"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XVisualIDFromVisual));
	proc_setdlsym(proc, STRV("libX11.so"), STRV("XCreateImage"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XCreateImage));
	proc_setdlsym(proc, STRV("libX11.so"), STRV("XDestroyImage"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XDestroyImage));
	proc_setdlsym(proc, STRV("libX11.so"), STRV("XPutImage"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XPutImage));
	proc_setdlsym(proc, STRV("libX11.so"), STRV("XFlush"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XFlush));
}

static int t_surface_swx_open(surface_t *surface)
{
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	t_surface_swx_symbols(&t_proc);
	t_display = (display_t){
		.drv  = &t_surface_swx_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_surface_swx_gfx_driver,
	};
	return surface_init(surface,
			    &(surface_config_t){
				    .display = &t_display,
				    .gfx     = &t_gfx,
			    },
			    ALLOC_STD) == NULL;
}

static int t_surface_swx_open_bound(surface_t *surface)
{
	if (t_surface_swx_open(surface)) {
		return 1;
	}

	t_window = (window_t){
		.display = &t_display,
	};
	return surface_bind(surface, &t_window);
}

static void t_surface_swx_close(surface_t *surface)
{
	surface_free(surface);
	proc_free(&t_proc);
}

static int t_surface_swx_memory(surface_t *surface, gfx_surface_memory_t *memory)
{
	surface_native_t native = {0};
	if (surface_native(surface, &native) || native.gfx_surface == NULL || native.gfx_surface->ops == NULL ||
	    native.gfx_surface->ops->memory == NULL) {
		return 1;
	}

	return native.gfx_surface->ops->memory(native.gfx_surface, memory);
}

TEST(surface_swx_driver_is_registered)
{
	START;

	EXPECT_NOT_NULL(t_surface_swx_driver());

	END;
}

TEST(surface_swx_compatible_accepts_x11_software)
{
	START;

	surface_driver_t *drv = t_surface_swx_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->compatible(&(surface_info_t){.gfx_api = GFX_API_SOFTWARE, .native_type = DISPLAY_NATIVE_X11}), 1);

	END;
}

TEST(surface_swx_compatible_rejects_invalid_info)
{
	START;

	surface_driver_t *drv = t_surface_swx_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->compatible(NULL), 0);
	EXPECT_EQ(drv->compatible(&(surface_info_t){.gfx_api = GFX_API_OPENGL, .native_type = DISPLAY_NATIVE_X11}), 0);
	EXPECT_EQ(drv->compatible(&(surface_info_t){.gfx_api = GFX_API_SOFTWARE, .native_type = DISPLAY_NATIVE_WINDOWS}), 0);

	END;
}

TEST(surface_swx_init_rejects_invalid_arguments)
{
	START;

	surface_driver_t *drv = t_surface_swx_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->init(NULL, &(surface_config_t){0}), 1);
	EXPECT_EQ(drv->init(&(surface_t){0}, NULL), 1);
	EXPECT_EQ(drv->init(&(surface_t){0}, &(surface_config_t){0}), 1);

	END;
}

TEST(surface_swx_init_alloc_failure)
{
	START;

	t_surface_swx_reset();
	t_alloc_fail_at = 1;
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	t_surface_swx_symbols(&t_proc);
	t_display = (display_t){
		.drv  = &t_surface_swx_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_surface_swx_gfx_driver,
	};
	surface_t surface = {0};
	log_set_quiet(0, 1);
	EXPECT_NULL(surface_init(&surface,
				 &(surface_config_t){
					 .display = &t_display,
					 .gfx	  = &t_gfx,
				 },
				 (alloc_t){.alloc = t_surface_swx_alloc, .free = alloc_free_std}));
	log_set_quiet(0, 0);

	proc_free(&t_proc);
	END;
}

TEST(surface_swx_init_loads_fallback_library)
{
	START;

	t_surface_swx_reset();
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	t_surface_swx_symbols_fallback(&t_proc);
	t_display = (display_t){
		.drv  = &t_surface_swx_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_surface_swx_gfx_driver,
	};
	surface_t surface = {0};
	EXPECT_PTR(surface_init(&surface,
				&(surface_config_t){
					.display = &t_display,
					.gfx	 = &t_gfx,
				},
				ALLOC_STD),
		   &surface);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_init_rejects_missing_library)
{
	START;

	t_surface_swx_reset();
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	t_display = (display_t){
		.drv  = &t_surface_swx_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_surface_swx_gfx_driver,
	};
	surface_t surface = {0};
	log_set_quiet(0, 1);
	EXPECT_NULL(surface_init(&surface,
				 &(surface_config_t){
					 .display = &t_display,
					 .gfx	  = &t_gfx,
				 },
				 ALLOC_STD));
	log_set_quiet(0, 0);

	proc_free(&t_proc);
	END;
}

TEST(surface_swx_init_rejects_missing_symbol)
{
	START;

	t_surface_swx_reset();
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	proc_setdlsym(&t_proc, STRV("libX11.so.6"), STRV("XDefaultVisual"), t_surface_swx_symbol((t_surface_swx_symbol_t)t_XDefaultVisual));
	t_display = (display_t){
		.drv  = &t_surface_swx_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_surface_swx_gfx_driver,
	};
	surface_t surface = {0};
	log_set_quiet(0, 1);
	EXPECT_NULL(surface_init(&surface,
				 &(surface_config_t){
					 .display = &t_display,
					 .gfx	  = &t_gfx,
				 },
				 ALLOC_STD));
	log_set_quiet(0, 0);

	proc_free(&t_proc);
	END;
}

TEST(surface_swx_free_rejects_invalid_arguments)
{
	START;

	surface_driver_t *drv = t_surface_swx_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->free(NULL), 1);
	EXPECT_EQ(drv->free(&(surface_t){0}), 1);

	END;
}

TEST(surface_swx_unbind_rejects_invalid_arguments)
{
	START;

	surface_driver_t *drv = t_surface_swx_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->unbind(NULL), 1);
	EXPECT_EQ(drv->unbind(&(surface_t){0}), 1);

	END;
}

TEST(surface_swx_config_window_rejects_invalid_arguments)
{
	START;

	surface_driver_t *drv = t_surface_swx_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->config_window(NULL, &(window_config_t){0}), 1);
	EXPECT_EQ(drv->config_window(&(surface_t){0}, NULL), 1);

	END;
}

TEST(surface_swx_config_window_rejects_missing_native_display)
{
	START;

	t_surface_swx_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_swx_open(&surface), 0);
	t_display_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_config_window(&surface, &(window_config_t){0}), 1);
	log_set_quiet(0, 0);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_config_window_rejects_wrong_native_type)
{
	START;

	t_surface_swx_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_swx_open(&surface), 0);
	t_display_native_type = DISPLAY_NATIVE_WINDOWS;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_config_window(&surface, &(window_config_t){0}), 1);
	log_set_quiet(0, 0);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_config_window_rejects_missing_x11_default)
{
	START;

	t_surface_swx_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_swx_open(&surface), 0);
	t_default_visual_ret = NULL;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_config_window(&surface, &(window_config_t){0}), 1);
	log_set_quiet(0, 0);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_config_window_sets_x11_visual)
{
	START;

	t_surface_swx_reset();
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_swx_open(&surface), 0);

	EXPECT_EQ(surface_config_window(&surface, &config), 0);
	EXPECT_EQ(config.depth, 24);
	EXPECT_EQ(config.visual, 0x87654321);
	EXPECT_EQ(config.background, WINDOW_BACKGROUND_NONE);
	EXPECT_EQ(t_visual_id_from_visual_calls, 1);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_bind_rejects_invalid_arguments)
{
	START;

	surface_driver_t *drv = t_surface_swx_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->bind(NULL, &t_window), 1);
	EXPECT_EQ(drv->bind(&(surface_t){0}, NULL), 1);

	END;
}

TEST(surface_swx_bind_rejects_missing_native_display)
{
	START;

	t_surface_swx_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_swx_open(&surface), 0);
	t_display_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_bind_rejects_missing_window_native)
{
	START;

	t_surface_swx_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_swx_open(&surface), 0);
	t_window_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_bind_rejects_wrong_window_native_type)
{
	START;

	t_surface_swx_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_swx_open(&surface), 0);
	t_window_native_type = DISPLAY_NATIVE_WINDOWS;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_bind_success)
{
	START;

	t_surface_swx_reset();
	surface_t surface	= {0};
	surface_native_t native = {0};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);

	EXPECT_EQ(surface_native(&surface, &native), 0);
	EXPECT_EQ(native.gfx_api, GFX_API_SOFTWARE);
	EXPECT_EQ(native.native_type, DISPLAY_NATIVE_X11);
	EXPECT_PTR(native.display, t_display_native_display);
	EXPECT_PTR(native.visual, &t_visual);
	EXPECT_EQ(native.handle, t_window_native_window);
	EXPECT_NOT_NULL(native.gfx_surface);
	EXPECT_EQ(native.gfx_surface->api, GFX_API_SOFTWARE);
	EXPECT_EQ(native.gfx_surface->handle, t_window_native_window);
	EXPECT_NOT_NULL(native.gfx_surface->data);
	EXPECT_NOT_NULL(native.gfx_surface->ops);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_bind_replaces_existing_window)
{
	START;

	t_surface_swx_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	t_window_native_window = 0x9876;

	EXPECT_EQ(surface_bind(&surface, &t_window), 0);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_native_rejects_invalid_arguments)
{
	START;

	t_surface_swx_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	EXPECT_EQ(surface.drv->native(NULL, &(surface_native_t){0}), 1);
	EXPECT_EQ(surface.drv->native(&surface, NULL), 1);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_native_rejects_unbound_surface)
{
	START;

	t_surface_swx_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_swx_open(&surface), 0);

	EXPECT_EQ(surface_native(&surface, &(surface_native_t){0}), 1);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_memory_rejects_invalid_arguments)
{
	START;

	t_surface_swx_reset();
	surface_t surface	    = {0};
	surface_native_t native	    = {0};
	gfx_surface_memory_t memory = {.width = 1, .height = 1};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->memory(NULL, &memory), 1);
	EXPECT_EQ(native.gfx_surface->ops->memory(native.gfx_surface, NULL), 1);
	memory.width = 0;
	EXPECT_EQ(native.gfx_surface->ops->memory(native.gfx_surface, &memory), 1);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_memory_rejects_unbound_surface)
{
	START;

	t_surface_swx_reset();
	surface_t surface	    = {0};
	surface_native_t native	    = {0};
	gfx_surface_memory_t memory = {.width = 1, .height = 1};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	EXPECT_EQ(surface_native(&surface, &native), 0);
	gfx_surface_t gfx_surface = *native.gfx_surface;
	EXPECT_EQ(surface_unbind(&surface), 0);

	EXPECT_EQ(gfx_surface.ops->memory(&gfx_surface, &memory), 1);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_memory_allocates_pixels)
{
	START;

	t_surface_swx_reset();
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);

	EXPECT_EQ(t_surface_swx_memory(&surface, &memory), 0);
	EXPECT_EQ(memory.format, GFX_FORMAT_RGBA8);
	EXPECT_NOT_NULL(memory.data);
	EXPECT_EQ(memory.width, 2);
	EXPECT_EQ(memory.height, 2);
	EXPECT_EQ(memory.stride, 8);
	EXPECT_EQ(t_create_image_calls, 1);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_memory_reuses_matching_image)
{
	START;

	t_surface_swx_reset();
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	EXPECT_EQ(t_surface_swx_memory(&surface, &memory), 0);
	void *pixels = memory.data;

	EXPECT_EQ(t_surface_swx_memory(&surface, &memory), 0);
	EXPECT_PTR(memory.data, pixels);
	EXPECT_EQ(t_create_image_calls, 1);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_memory_replaces_resized_image)
{
	START;

	t_surface_swx_reset();
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	EXPECT_EQ(t_surface_swx_memory(&surface, &memory), 0);
	memory.width  = 1;
	memory.height = 1;

	EXPECT_EQ(t_surface_swx_memory(&surface, &memory), 0);
	EXPECT_EQ(t_destroy_image_calls, 1);
	EXPECT_EQ(t_create_image_calls, 2);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_memory_pixels_alloc_failure)
{
	START;

	t_surface_swx_reset();
	t_alloc_fail_at = 2;
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	t_surface_swx_symbols(&t_proc);
	t_display	  = (display_t){.drv = &t_surface_swx_display_driver, .proc = &t_proc};
	t_gfx		  = (gfx_t){.drv = &t_surface_swx_gfx_driver};
	surface_t surface = {0};
	EXPECT_PTR(surface_init(&surface,
				&(surface_config_t){
					.display = &t_display,
					.gfx	 = &t_gfx,
				},
				(alloc_t){.alloc = t_surface_swx_alloc, .free = alloc_free_std}),
		   &surface);
	t_window = (window_t){.display = &t_display};
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(t_surface_swx_memory(&surface, &(gfx_surface_memory_t){.width = 2, .height = 2}), 1);
	log_set_quiet(0, 0);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_memory_create_image_null)
{
	START;

	t_surface_swx_reset();
	t_create_image_null = 1;
	surface_t surface   = {0};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(t_surface_swx_memory(&surface, &(gfx_surface_memory_t){.width = 2, .height = 2}), 1);
	log_set_quiet(0, 0);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_memory_create_image_invalid)
{
	START;

	t_surface_swx_reset();
	t_create_image_bad_bits = 1;
	surface_t surface	= {0};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(t_surface_swx_memory(&surface, &(gfx_surface_memory_t){.width = 2, .height = 2}), 1);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_destroy_image_calls, 1);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_memory_image_data_alloc_failure)
{
	START;

	t_surface_swx_reset();
	t_alloc_fail_at = 3;
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	t_surface_swx_symbols(&t_proc);
	t_display	  = (display_t){.drv = &t_surface_swx_display_driver, .proc = &t_proc};
	t_gfx		  = (gfx_t){.drv = &t_surface_swx_gfx_driver};
	surface_t surface = {0};
	EXPECT_PTR(surface_init(&surface,
				&(surface_config_t){
					.display = &t_display,
					.gfx	 = &t_gfx,
				},
				(alloc_t){.alloc = t_surface_swx_alloc, .free = alloc_free_std}),
		   &surface);
	t_window = (window_t){.display = &t_display};
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(t_surface_swx_memory(&surface, &(gfx_surface_memory_t){.width = 2, .height = 2}), 1);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_destroy_image_calls, 1);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_present_rejects_invalid_arguments)
{
	START;

	t_surface_swx_reset();
	surface_t surface	= {0};
	surface_native_t native = {0};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->present(NULL, GFX_PRESENT_MODE_DEFAULT), 1);
	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT), 1);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_present_flush_failure)
{
	START;

	t_surface_swx_reset();
	t_flush_ret		    = 0;
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	surface_native_t native	    = {0};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	EXPECT_EQ(t_surface_swx_memory(&surface, &memory), 0);
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT), 1);
	EXPECT_EQ(t_put_image_calls, 1);
	EXPECT_EQ(t_flush_calls, 1);
	EXPECT_EQ(t_put_image_window, t_window_native_window);
	EXPECT_EQ(t_put_image_width, 2);
	EXPECT_EQ(t_put_image_height, 2);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_present_converts_rgba_to_ximage)
{
	START;

	t_surface_swx_reset();
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	surface_native_t native	    = {0};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	EXPECT_EQ(t_surface_swx_memory(&surface, &memory), 0);
	u8 *pixels = memory.data;
	pixels[0]  = 0x11;
	pixels[1]  = 0x22;
	pixels[2]  = 0x33;
	pixels[4]  = 0x44;
	pixels[5]  = 0x55;
	pixels[6]  = 0x66;
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT), 0);
	EXPECT_EQ(t_put_image_calls, 1);
	EXPECT_EQ(t_flush_calls, 1);
	EXPECT_EQ(t_put_image_window, t_window_native_window);
	EXPECT_EQ(t_put_image_width, 2);
	EXPECT_EQ(t_put_image_height, 2);
	EXPECT_EQ((u8)t_image.data[0], 0x33);
	EXPECT_EQ((u8)t_image.data[1], 0x22);
	EXPECT_EQ((u8)t_image.data[2], 0x11);
	EXPECT_EQ((u8)t_image.data[4], 0x66);
	EXPECT_EQ((u8)t_image.data[5], 0x55);
	EXPECT_EQ((u8)t_image.data[6], 0x44);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_present_converts_big_endian_masks)
{
	START;

	t_surface_swx_reset();
	t_image.byte_order	    = 1;
	t_image.bits_per_pixel	    = 32;
	t_image.bytes_per_line	    = 8;
	t_image.red_mask	    = 0xFF000000;
	t_image.green_mask	    = 0;
	t_image.blue_mask	    = 0;
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	surface_native_t native	    = {0};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	EXPECT_EQ(t_surface_swx_memory(&surface, &memory), 0);
	u8 *pixels = memory.data;
	pixels[0]  = 0xFF;
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT), 0);
	EXPECT_EQ((u8)t_image.data[0], 0xFF);

	t_surface_swx_close(&surface);
	END;
}

TEST(surface_swx_gfx_present_converts_wide_zero_channel)
{
	START;

	t_surface_swx_reset();
	t_image.bits_per_pixel	    = 64;
	t_image.bytes_per_line	    = 16;
	t_image.red_mask	    = ~0ul;
	t_image.green_mask	    = 0;
	t_image.blue_mask	    = 0;
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	surface_native_t native	    = {0};
	EXPECT_EQ(t_surface_swx_open_bound(&surface), 0);
	EXPECT_EQ(t_surface_swx_memory(&surface, &memory), 0);
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT), 0);
	EXPECT_EQ((u8)t_image.data[0], 0);
	EXPECT_EQ((u8)t_image.data[7], 0);

	t_surface_swx_close(&surface);
	END;
}

STEST(surface_swx)
{
	SSTART;

	RUN(surface_swx_driver_is_registered);
	RUN(surface_swx_compatible_accepts_x11_software);
	RUN(surface_swx_compatible_rejects_invalid_info);
	RUN(surface_swx_init_rejects_invalid_arguments);
	RUN(surface_swx_init_alloc_failure);
	RUN(surface_swx_init_loads_fallback_library);
	RUN(surface_swx_init_rejects_missing_library);
	RUN(surface_swx_init_rejects_missing_symbol);
	RUN(surface_swx_free_rejects_invalid_arguments);
	RUN(surface_swx_unbind_rejects_invalid_arguments);
	RUN(surface_swx_config_window_rejects_invalid_arguments);
	RUN(surface_swx_config_window_rejects_missing_native_display);
	RUN(surface_swx_config_window_rejects_wrong_native_type);
	RUN(surface_swx_config_window_rejects_missing_x11_default);
	RUN(surface_swx_config_window_sets_x11_visual);
	RUN(surface_swx_bind_rejects_invalid_arguments);
	RUN(surface_swx_bind_rejects_missing_native_display);
	RUN(surface_swx_bind_rejects_missing_window_native);
	RUN(surface_swx_bind_rejects_wrong_window_native_type);
	RUN(surface_swx_bind_success);
	RUN(surface_swx_bind_replaces_existing_window);
	RUN(surface_swx_native_rejects_invalid_arguments);
	RUN(surface_swx_native_rejects_unbound_surface);
	RUN(surface_swx_gfx_memory_rejects_invalid_arguments);
	RUN(surface_swx_gfx_memory_rejects_unbound_surface);
	RUN(surface_swx_gfx_memory_allocates_pixels);
	RUN(surface_swx_gfx_memory_reuses_matching_image);
	RUN(surface_swx_gfx_memory_replaces_resized_image);
	RUN(surface_swx_gfx_memory_pixels_alloc_failure);
	RUN(surface_swx_gfx_memory_create_image_null);
	RUN(surface_swx_gfx_memory_create_image_invalid);
	RUN(surface_swx_gfx_memory_image_data_alloc_failure);
	RUN(surface_swx_gfx_present_rejects_invalid_arguments);
	RUN(surface_swx_gfx_present_flush_failure);
	RUN(surface_swx_gfx_present_converts_rgba_to_ximage);
	RUN(surface_swx_gfx_present_converts_big_endian_masks);
	RUN(surface_swx_gfx_present_converts_wide_zero_channel);

	SEND;
}

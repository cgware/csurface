#include "surface_driver.h"

#include "display_driver.h"
#include "gfx_driver.h"
#include "log.h"
#include "test.h"

typedef void *HDC;
typedef void *HWND;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef int BOOL;

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
	T_PFD_DOUBLEBUFFER    = 0x00000001,
	T_PFD_DRAW_TO_WINDOW  = 0x00000004,
	T_PFD_SUPPORT_OPENGL  = 0x00000020,
	T_PFD_TYPE_RGBA       = 0,
};

typedef void (*t_surface_wgl_symbol_t)(void);

static int t_wgl_get_dc_calls;
static int t_wgl_release_dc_calls;
static int t_wgl_choose_pixel_format_calls;
static int t_wgl_set_pixel_format_calls;
static int t_wgl_get_pixel_format_calls;
static int t_wgl_describe_pixel_format_calls;
static int t_wgl_get_proc_address_calls;
static int t_wgl_create_context_calls;
static int t_wgl_delete_context_calls;
static int t_wgl_make_current_calls;
static int t_wgl_swap_buffers_calls;
static HWND t_wgl_window;
static HDC t_wgl_dc;
static void *t_wgl_context;
static int t_wgl_pixel_format;
static HDC t_wgl_get_dc_ret;
static int t_wgl_release_dc_ret;
static int t_wgl_choose_pixel_format_ret;
static BOOL t_wgl_set_pixel_format_ret;
static int t_wgl_get_pixel_format_ret;
static int t_wgl_describe_pixel_format_ret;
static void *t_wgl_get_proc_address_ret;
static void *t_wgl_create_context_ret;
static BOOL t_wgl_delete_context_ret;
static BOOL t_wgl_make_current_ret;
static BOOL t_wgl_swap_buffers_ret;
static PIXELFORMATDESCRIPTOR t_wgl_describe_pixel_format_value;
static int t_display_native_ret;
static display_native_type_t t_display_native_type;
static void *t_display_native_display;
static int t_window_native_ret;
static display_native_type_t t_window_native_type;
static void *t_window_native_window;

static void *t_surface_wgl_symbol(t_surface_wgl_symbol_t fn)
{
	union {
		t_surface_wgl_symbol_t fn;
		void *ptr;
	} symbol = {.fn = fn};

	return symbol.ptr;
}

static HDC t_GetDC(HWND hwnd)
{
	t_wgl_get_dc_calls++;
	t_wgl_window = hwnd;
	return t_wgl_get_dc_ret;
}

static int t_ReleaseDC(HWND hwnd, HDC dc)
{
	t_wgl_release_dc_calls++;
	t_wgl_window = hwnd;
	t_wgl_dc     = dc;
	return t_wgl_release_dc_ret;
}

static int t_ChoosePixelFormat(HDC dc, const PIXELFORMATDESCRIPTOR *format)
{
	(void)format;
	t_wgl_choose_pixel_format_calls++;
	t_wgl_dc = dc;
	return t_wgl_choose_pixel_format_ret;
}

static BOOL t_SetPixelFormat(HDC dc, int pixel_format, const PIXELFORMATDESCRIPTOR *format)
{
	(void)format;
	t_wgl_set_pixel_format_calls++;
	t_wgl_dc		  = dc;
	t_wgl_pixel_format = pixel_format;
	return t_wgl_set_pixel_format_ret;
}

static int t_GetPixelFormat(HDC dc)
{
	t_wgl_get_pixel_format_calls++;
	t_wgl_dc = dc;
	return t_wgl_get_pixel_format_ret;
}

static int t_DescribePixelFormat(HDC dc, int pixel_format, UINT size, PIXELFORMATDESCRIPTOR *format)
{
	(void)size;
	t_wgl_describe_pixel_format_calls++;
	t_wgl_dc		  = dc;
	t_wgl_pixel_format = pixel_format;
	*format		  = t_wgl_describe_pixel_format_value;
	return t_wgl_describe_pixel_format_ret;
}

static void *t_wglGetProcAddress(const char *name)
{
	(void)name;
	t_wgl_get_proc_address_calls++;
	return t_wgl_get_proc_address_ret;
}

static void *t_wglCreateContext(HDC dc)
{
	t_wgl_create_context_calls++;
	t_wgl_dc = dc;
	return t_wgl_create_context_ret;
}

static BOOL t_wglDeleteContext(void *context)
{
	t_wgl_delete_context_calls++;
	t_wgl_context = context;
	return t_wgl_delete_context_ret;
}

static BOOL t_wglMakeCurrent(HDC dc, void *context)
{
	t_wgl_make_current_calls++;
	t_wgl_dc      = dc;
	t_wgl_context = context;
	return t_wgl_make_current_ret;
}

static BOOL t_SwapBuffers(HDC dc)
{
	t_wgl_swap_buffers_calls++;
	t_wgl_dc = dc;
	return t_wgl_swap_buffers_ret;
}

static int t_surface_wgl_display_native(display_t *display, display_native_t *native)
{
	(void)display;
	if (t_display_native_ret) {
		return t_display_native_ret;
	}
	*native = (display_native_t){
		.type	 = t_display_native_type,
		.display = t_display_native_display,
	};
	return 0;
}

static int t_surface_wgl_window_native(window_t *window, window_native_t *native)
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

static display_driver_t t_surface_wgl_display_driver = {
	.name	       = "test",
	.native	       = t_surface_wgl_display_native,
	.window_native = t_surface_wgl_window_native,
};

static gfx_driver_t t_surface_wgl_gfx_driver = {
	.name = "test",
	.api  = GFX_API_OPENGL,
};

static surface_driver_t *t_surface_wgl_driver(void)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type == SURFACE_DRIVER_TYPE) {
			surface_driver_t *drv = i->data;
			if (strv_eq(strv_cstr(drv->name), STRV("wgl"))) {
				return drv;
			}
		}
	}

	return NULL;
}

static void t_surface_wgl_reset(void)
{
	t_wgl_get_dc_calls		 = 0;
	t_wgl_release_dc_calls		 = 0;
	t_wgl_choose_pixel_format_calls = 0;
	t_wgl_set_pixel_format_calls	 = 0;
	t_wgl_get_pixel_format_calls	 = 0;
	t_wgl_describe_pixel_format_calls = 0;
	t_wgl_get_proc_address_calls	 = 0;
	t_wgl_create_context_calls	 = 0;
	t_wgl_delete_context_calls	 = 0;
	t_wgl_make_current_calls	 = 0;
	t_wgl_swap_buffers_calls	 = 0;
	t_wgl_window			 = NULL;
	t_wgl_dc			 = NULL;
	t_wgl_context			 = NULL;
	t_wgl_pixel_format		 = 0;
	t_wgl_get_dc_ret		 = (HDC)0x5678;
	t_wgl_release_dc_ret		 = 1;
	t_wgl_choose_pixel_format_ret	 = 7;
	t_wgl_set_pixel_format_ret	 = 1;
	t_wgl_get_pixel_format_ret	 = 0;
	t_wgl_describe_pixel_format_ret	 = 1;
	t_wgl_get_proc_address_ret	 = (void *)0x8765;
	t_wgl_create_context_ret	 = (void *)0x3456;
	t_wgl_delete_context_ret	 = 1;
	t_wgl_make_current_ret		 = 1;
	t_wgl_swap_buffers_ret		 = 1;
	t_wgl_describe_pixel_format_value = (PIXELFORMATDESCRIPTOR){
		.dwFlags    = T_PFD_DRAW_TO_WINDOW | T_PFD_SUPPORT_OPENGL | T_PFD_DOUBLEBUFFER,
		.iPixelType = T_PFD_TYPE_RGBA,
	};
	t_display_native_ret	 = 0;
	t_display_native_type	 = DISPLAY_NATIVE_WINDOWS;
	t_display_native_display = (void *)0x1111;
	t_window_native_ret	 = 0;
	t_window_native_type	 = DISPLAY_NATIVE_WINDOWS;
	t_window_native_window	 = (void *)0x1234;
}

static void t_surface_wgl_symbols(proc_t *proc)
{
	proc_setdlsym(proc, STRV("user32.dll"), STRV("GetDC"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_GetDC));
	proc_setdlsym(proc, STRV("user32.dll"), STRV("ReleaseDC"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_ReleaseDC));
	proc_setdlsym(
		proc, STRV("gdi32.dll"), STRV("ChoosePixelFormat"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_ChoosePixelFormat));
	proc_setdlsym(proc, STRV("gdi32.dll"), STRV("SetPixelFormat"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_SetPixelFormat));
	proc_setdlsym(proc, STRV("gdi32.dll"), STRV("GetPixelFormat"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_GetPixelFormat));
	proc_setdlsym(
		proc, STRV("gdi32.dll"), STRV("DescribePixelFormat"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_DescribePixelFormat));
	proc_setdlsym(proc, STRV("gdi32.dll"), STRV("SwapBuffers"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_SwapBuffers));
	proc_setdlsym(
		proc, STRV("opengl32.dll"), STRV("wglGetProcAddress"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_wglGetProcAddress));
	proc_setdlsym(
		proc, STRV("opengl32.dll"), STRV("wglCreateContext"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_wglCreateContext));
	proc_setdlsym(
		proc, STRV("opengl32.dll"), STRV("wglDeleteContext"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_wglDeleteContext));
	proc_setdlsym(
		proc, STRV("opengl32.dll"), STRV("wglMakeCurrent"), t_surface_wgl_symbol((t_surface_wgl_symbol_t)t_wglMakeCurrent));
}

static int t_surface_wgl_open(proc_t *proc, gfx_t *gfx, display_t *display, surface_t *surface)
{
	proc_init(proc, 0, 1, ALLOC_STD);
	t_surface_wgl_symbols(proc);
	*gfx = (gfx_t){
		.drv = &t_surface_wgl_gfx_driver,
	};
	*display = (display_t){
		.drv  = &t_surface_wgl_display_driver,
		.proc = proc,
	};

	return surface_init(surface,
			    &(surface_config_t){
				    .display = display,
				    .gfx     = gfx,
				    .alloc   = ALLOC_STD,
			    }) == NULL;
}

static void t_surface_wgl_close(proc_t *proc, surface_t *surface)
{
	surface_free(surface);
	proc_free(proc);
}

TEST(surface_wgl_driver_is_registered)
{
	START;

	EXPECT_NE(t_surface_wgl_driver(), NULL);

	END;
}

TEST(surface_wgl_init_rejects_non_opengl)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {.drv = &t_surface_wgl_gfx_driver};
	display_t display = {.drv = &t_surface_wgl_display_driver, .proc = &proc};
	surface_t surface = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);
	t_surface_wgl_symbols(&proc);
	t_display_native_type = DISPLAY_NATIVE_X11;

	EXPECT_EQ(surface_init(&surface, &(surface_config_t){.display = &display, .gfx = &gfx, .alloc = ALLOC_STD}), NULL);

	proc_free(&proc);
	END;
}

TEST(surface_wgl_init_missing_symbol)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {.drv = &t_surface_wgl_gfx_driver};
	display_t display = {.drv = &t_surface_wgl_display_driver, .proc = &proc};
	surface_t surface = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_init_driver(
			  &surface, t_surface_wgl_driver(), &(surface_config_t){.display = &display, .gfx = &gfx, .alloc = ALLOC_STD}),
		  NULL);
	log_set_quiet(0, 0);

	proc_free(&proc);
	END;
}

TEST(surface_wgl_config_window_sets_defaults)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_config_t config = {.depth = 24, .visual = 1};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);

	EXPECT_EQ(surface_config_window(&surface, &config), 0);
	EXPECT_EQ(config.depth, 0);
	EXPECT_EQ(config.visual, 0);
	EXPECT_EQ(config.background, WINDOW_BACKGROUND_NONE);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_config_window_native_unavailable)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);
	t_display_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_config_window(&surface, &config), 1);
	log_set_quiet(0, 0);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_bind_sets_pixel_format)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window = {.display = &display};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);

	EXPECT_EQ(surface_bind(&surface, &window), 0);
	EXPECT_EQ(t_wgl_choose_pixel_format_calls, 1);
	EXPECT_EQ(t_wgl_set_pixel_format_calls, 1);
	EXPECT_EQ(t_wgl_pixel_format, 7);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_bind_uses_existing_pixel_format)
{
	START;

	t_surface_wgl_reset();
	t_wgl_get_pixel_format_ret = 9;
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window = {.display = &display};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);

	EXPECT_EQ(surface_bind(&surface, &window), 0);
	EXPECT_EQ(t_wgl_describe_pixel_format_calls, 1);
	EXPECT_EQ(t_wgl_set_pixel_format_calls, 0);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_bind_rejects_unsupported_existing_pixel_format)
{
	START;

	t_surface_wgl_reset();
	t_wgl_get_pixel_format_ret = 9;
	t_wgl_describe_pixel_format_value.dwFlags = T_PFD_DRAW_TO_WINDOW;
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window = {.display = &display};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_wgl_release_dc_calls, 1);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_bind_get_dc_failure)
{
	START;

	t_surface_wgl_reset();
	t_wgl_get_dc_ret = NULL;
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window = {.display = &display};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_bind_window_unavailable)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window = {.display = &display};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);
	t_window_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_bind_replaces_dc)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window = {.display = &display};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);
	surface_bind(&surface, &window);
	t_wgl_get_dc_ret = (HDC)0x9876;

	EXPECT_EQ(surface_bind(&surface, &window), 0);
	EXPECT_EQ(t_wgl_release_dc_calls, 1);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_native_returns_handles)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window = {.display = &display};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);
	surface_bind(&surface, &window);

	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);
	EXPECT_EQ(native.display, (void *)0x5678);
	EXPECT_EQ(native.handle, (u64)(uintptr_t)t_window_native_window);
	EXPECT_EQ(native.visual, (void *)(uintptr_t)7);
	EXPECT_NE(native.gfx_surface, NULL);
	EXPECT_EQ(native.gfx_surface->api, GFX_API_OPENGL);
	EXPECT_EQ(native.gfx_surface->handle, (u64)(uintptr_t)t_window_native_window);
	EXPECT_NE(native.gfx_surface->data, NULL);
	EXPECT_NE(native.gfx_surface->ops, NULL);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_native_without_bind)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);

	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 1);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_unbind_releases_dc)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window = {.display = &display};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);
	surface_bind(&surface, &window);

	EXPECT_EQ(surface_unbind(&surface), 0);
	EXPECT_EQ(t_wgl_release_dc_calls, 1);
	EXPECT_EQ(t_wgl_dc, (HDC)0x5678);

	t_surface_wgl_close(&proc, &surface);
	END;
}

TEST(surface_wgl_free_releases_dc)
{
	START;

	t_surface_wgl_reset();
	proc_t proc = {0};
	gfx_t gfx = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window = {.display = &display};
	EXPECT_EQ(t_surface_wgl_open(&proc, &gfx, &display, &surface), 0);
	surface_bind(&surface, &window);

	surface_free(&surface);
	EXPECT_EQ(t_wgl_release_dc_calls, 1);

	proc_free(&proc);
	END;
}

STEST(surface_wgl)
{
	SSTART;

	RUN(surface_wgl_driver_is_registered);
	RUN(surface_wgl_init_rejects_non_opengl);
	RUN(surface_wgl_init_missing_symbol);
	RUN(surface_wgl_config_window_sets_defaults);
	RUN(surface_wgl_config_window_native_unavailable);
	RUN(surface_wgl_bind_sets_pixel_format);
	RUN(surface_wgl_bind_uses_existing_pixel_format);
	RUN(surface_wgl_bind_rejects_unsupported_existing_pixel_format);
	RUN(surface_wgl_bind_get_dc_failure);
	RUN(surface_wgl_bind_window_unavailable);
	RUN(surface_wgl_bind_replaces_dc);
	RUN(surface_wgl_native_returns_handles);
	RUN(surface_wgl_native_without_bind);
	RUN(surface_wgl_unbind_releases_dc);
	RUN(surface_wgl_free_releases_dc);

	SEND;
}

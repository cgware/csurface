#include "log.h"
#include "surface_driver.h"

#include "display_driver.h"
#include "gfx_driver.h"
#include "mem.h"
#include "test.h"

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

typedef void (*t_surface_wsw_symbol_t)(void);

static int t_wsw_get_dc_calls;
static int t_wsw_release_dc_calls;
static int t_wsw_create_compatible_dc_calls;
static int t_wsw_delete_dc_calls;
static int t_wsw_create_dib_section_calls;
static int t_wsw_select_object_calls;
static int t_wsw_delete_object_calls;
static int t_wsw_bit_blt_calls;
static HWND t_wsw_window;
static HDC t_wsw_dc;
static HDC t_wsw_memory_dc;
static HGDIOBJ t_wsw_selected_object;
static int t_wsw_bit_blt_width;
static int t_wsw_bit_blt_height;
static HDC t_wsw_get_dc_ret;
static HDC t_wsw_create_compatible_dc_ret;
static HBITMAP t_wsw_create_dib_section_ret;
static HGDIOBJ t_wsw_select_object_ret;
static BOOL t_wsw_bit_blt_ret;
static u8 t_wsw_bitmap_pixels[16];
static int t_wsw_create_dib_section_null_bits;
static int t_alloc_calls;
static int t_alloc_fail_at;
static int t_display_native_ret;
static display_native_type_t t_display_native_type;
static void *t_display_native_display;
static int t_window_native_ret;
static display_native_type_t t_window_native_type;
static void *t_window_native_window;
static proc_t t_proc;
static display_t t_display;
static gfx_t t_gfx;
static window_t t_window;

static void *t_surface_wsw_symbol(t_surface_wsw_symbol_t fn)
{
	union {
		t_surface_wsw_symbol_t fn;
		void *ptr;
	} symbol = {.fn = fn};

	return symbol.ptr;
}

static void *t_surface_wsw_alloc(alloc_t *alloc, size_t size)
{
	(void)alloc;
	t_alloc_calls++;
	if (t_alloc_fail_at == t_alloc_calls) {
		return NULL;
	}

	return alloc_alloc_std(NULL, size);
}

static HDC t_GetDC(HWND hwnd)
{
	t_wsw_get_dc_calls++;
	t_wsw_window = hwnd;
	return t_wsw_get_dc_ret;
}

static int t_ReleaseDC(HWND hwnd, HDC dc)
{
	t_wsw_release_dc_calls++;
	t_wsw_window = hwnd;
	t_wsw_dc     = dc;
	return 1;
}

static HDC t_CreateCompatibleDC(HDC dc)
{
	t_wsw_create_compatible_dc_calls++;
	t_wsw_dc = dc;
	return t_wsw_create_compatible_dc_ret;
}

static BOOL t_DeleteDC(HDC dc)
{
	t_wsw_delete_dc_calls++;
	t_wsw_memory_dc = dc;
	return 1;
}

static HBITMAP t_CreateDIBSection(HDC dc, const BITMAPINFO *info, UINT usage, void **bits, HANDLE section, DWORD offset)
{
	(void)usage;
	(void)section;
	(void)offset;
	t_wsw_create_dib_section_calls++;
	t_wsw_dc = dc;
	if (info == NULL || info->bmiHeader.biWidth != 2 || info->bmiHeader.biHeight != -2 || info->bmiHeader.biBitCount != 32) {
		return NULL;
	}
	if (t_wsw_create_dib_section_null_bits) {
		*bits = NULL;
		return t_wsw_create_dib_section_ret;
	}
	*bits = t_wsw_bitmap_pixels;
	return t_wsw_create_dib_section_ret;
}

static HGDIOBJ t_SelectObject(HDC dc, HGDIOBJ object)
{
	t_wsw_select_object_calls++;
	t_wsw_memory_dc	      = dc;
	t_wsw_selected_object = object;
	return t_wsw_select_object_ret;
}

static BOOL t_DeleteObject(HGDIOBJ object)
{
	(void)object;
	t_wsw_delete_object_calls++;
	return 1;
}

static BOOL t_BitBlt(HDC dc, int x, int y, int width, int height, HDC src_dc, int src_x, int src_y, DWORD rop)
{
	(void)x;
	(void)y;
	(void)src_x;
	(void)src_y;
	(void)rop;
	t_wsw_bit_blt_calls++;
	t_wsw_dc	     = dc;
	t_wsw_memory_dc	     = src_dc;
	t_wsw_bit_blt_width  = width;
	t_wsw_bit_blt_height = height;
	return t_wsw_bit_blt_ret;
}

static int t_surface_wsw_display_native(display_t *display, display_native_t *native)
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

static int t_surface_wsw_window_native(window_t *window, window_native_t *native)
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

static display_driver_t t_surface_wsw_display_driver = {
	.name	       = "test",
	.native	       = t_surface_wsw_display_native,
	.window_native = t_surface_wsw_window_native,
};

static gfx_driver_t t_surface_wsw_gfx_driver = {
	.name = "test",
	.api  = GFX_API_SOFTWARE,
};

static surface_driver_t *t_surface_wsw_driver(void)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type == SURFACE_DRIVER_TYPE) {
			surface_driver_t *drv = i->data;
			if (strv_eq(strv_cstr(drv->name), STRV("wsw"))) {
				return drv;
			}
		}
	}

	return NULL;
}

static void t_surface_wsw_reset(void)
{
	t_wsw_get_dc_calls		 = 0;
	t_wsw_release_dc_calls		 = 0;
	t_wsw_create_compatible_dc_calls = 0;
	t_wsw_delete_dc_calls		 = 0;
	t_wsw_create_dib_section_calls	 = 0;
	t_wsw_select_object_calls	 = 0;
	t_wsw_delete_object_calls	 = 0;
	t_wsw_bit_blt_calls		 = 0;
	t_wsw_window			 = NULL;
	t_wsw_dc			 = NULL;
	t_wsw_memory_dc			 = NULL;
	t_wsw_selected_object		 = NULL;
	t_wsw_bit_blt_width		 = 0;
	t_wsw_bit_blt_height		 = 0;
	t_wsw_get_dc_ret		 = (HDC)0x1111;
	t_wsw_create_compatible_dc_ret	 = (HDC)0x2222;
	t_wsw_create_dib_section_ret	 = (HBITMAP)0x3333;
	t_wsw_select_object_ret		 = (HGDIOBJ)0x4444;
	t_wsw_bit_blt_ret		 = 1;
	mem_set(t_wsw_bitmap_pixels, 0, sizeof(t_wsw_bitmap_pixels));
	t_wsw_create_dib_section_null_bits = 0;
	t_alloc_calls			   = 0;
	t_alloc_fail_at			   = 0;
	t_display_native_ret		   = 0;
	t_display_native_type		   = DISPLAY_NATIVE_WINDOWS;
	t_display_native_display	   = (void *)0x5555;
	t_window_native_ret		   = 0;
	t_window_native_type		   = DISPLAY_NATIVE_WINDOWS;
	t_window_native_window		   = (void *)0x6666;
	t_proc				   = (proc_t){0};
	t_display			   = (display_t){0};
	t_gfx				   = (gfx_t){0};
	t_window			   = (window_t){0};
}

static void t_surface_wsw_symbols(proc_t *proc)
{
	proc_setdlsym(proc, STRV("user32.dll"), STRV("GetDC"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_GetDC));
	proc_setdlsym(proc, STRV("user32.dll"), STRV("ReleaseDC"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_ReleaseDC));
	proc_setdlsym(
		proc, STRV("gdi32.dll"), STRV("CreateCompatibleDC"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_CreateCompatibleDC));
	proc_setdlsym(proc, STRV("gdi32.dll"), STRV("DeleteDC"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_DeleteDC));
	proc_setdlsym(proc, STRV("gdi32.dll"), STRV("CreateDIBSection"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_CreateDIBSection));
	proc_setdlsym(proc, STRV("gdi32.dll"), STRV("SelectObject"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_SelectObject));
	proc_setdlsym(proc, STRV("gdi32.dll"), STRV("DeleteObject"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_DeleteObject));
	proc_setdlsym(proc, STRV("gdi32.dll"), STRV("BitBlt"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_BitBlt));
}

static int t_surface_wsw_open(surface_t *surface)
{
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	t_surface_wsw_symbols(&t_proc);
	t_display = (display_t){
		.drv  = &t_surface_wsw_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_surface_wsw_gfx_driver,
	};
	return surface_init(surface,
			    &(surface_config_t){
				    .display = &t_display,
				    .gfx     = &t_gfx,
			    },
			    ALLOC_STD) == NULL;
}

static int t_surface_wsw_open_bound(surface_t *surface)
{
	if (t_surface_wsw_open(surface)) {
		return 1;
	}

	t_window = (window_t){
		.display = &t_display,
	};
	return surface_bind(surface, &t_window);
}

static void t_surface_wsw_close(surface_t *surface)
{
	surface_free(surface);
	proc_free(&t_proc);
}

static int t_surface_wsw_memory(surface_t *surface, gfx_surface_memory_t *memory)
{
	surface_native_t native = {0};
	if (surface_native(surface, &native) || native.gfx_surface == NULL || native.gfx_surface->ops == NULL ||
	    native.gfx_surface->ops->memory == NULL) {
		return 1;
	}

	return native.gfx_surface->ops->memory(native.gfx_surface, memory);
}

TEST(surface_wsw_driver_is_registered)
{
	START;

	EXPECT_NOT_NULL(t_surface_wsw_driver());

	END;
}

TEST(surface_wsw_compatible_accepts_windows_software)
{
	START;

	surface_driver_t *drv = t_surface_wsw_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->compatible(&(surface_info_t){.gfx_api = GFX_API_SOFTWARE, .native_type = DISPLAY_NATIVE_WINDOWS}), 1);

	END;
}

TEST(surface_wsw_compatible_rejects_invalid_info)
{
	START;

	surface_driver_t *drv = t_surface_wsw_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->compatible(NULL), 0);
	EXPECT_EQ(drv->compatible(&(surface_info_t){.gfx_api = GFX_API_OPENGL, .native_type = DISPLAY_NATIVE_WINDOWS}), 0);
	EXPECT_EQ(drv->compatible(&(surface_info_t){.gfx_api = GFX_API_SOFTWARE, .native_type = DISPLAY_NATIVE_X11}), 0);

	END;
}

TEST(surface_wsw_init_rejects_invalid_arguments)
{
	START;

	surface_driver_t *drv = t_surface_wsw_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->init(NULL, &(surface_config_t){0}), 1);
	EXPECT_EQ(drv->init(&(surface_t){0}, NULL), 1);
	EXPECT_EQ(drv->init(&(surface_t){0}, &(surface_config_t){0}), 1);

	END;
}

TEST(surface_wsw_init_alloc_failure)
{
	START;

	t_surface_wsw_reset();
	t_alloc_fail_at = 1;
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	t_surface_wsw_symbols(&t_proc);
	t_display = (display_t){
		.drv  = &t_surface_wsw_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_surface_wsw_gfx_driver,
	};
	surface_t surface = {0};

	log_set_quiet(0, 1);
	EXPECT_NULL(surface_init(&surface,
				 &(surface_config_t){
					 .display = &t_display,
					 .gfx	  = &t_gfx,
				 },
				 (alloc_t){.alloc = t_surface_wsw_alloc, .free = alloc_free_std}));
	log_set_quiet(0, 0);

	proc_free(&t_proc);
	END;
}

TEST(surface_wsw_init_rejects_missing_user32_library)
{
	START;

	t_surface_wsw_reset();
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	t_display = (display_t){
		.drv  = &t_surface_wsw_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_surface_wsw_gfx_driver,
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

TEST(surface_wsw_init_rejects_missing_gdi32_library)
{
	START;

	t_surface_wsw_reset();
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	proc_setdlsym(&t_proc, STRV("user32.dll"), STRV("GetDC"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_GetDC));
	t_display = (display_t){
		.drv  = &t_surface_wsw_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_surface_wsw_gfx_driver,
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

TEST(surface_wsw_init_rejects_missing_symbol)
{
	START;

	t_surface_wsw_reset();
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	proc_setdlsym(&t_proc, STRV("user32.dll"), STRV("GetDC"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_GetDC));
	proc_setdlsym(&t_proc, STRV("user32.dll"), STRV("ReleaseDC"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_ReleaseDC));
	proc_setdlsym(
		&t_proc, STRV("gdi32.dll"), STRV("CreateCompatibleDC"), t_surface_wsw_symbol((t_surface_wsw_symbol_t)t_CreateCompatibleDC));
	t_display = (display_t){
		.drv  = &t_surface_wsw_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_surface_wsw_gfx_driver,
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

TEST(surface_wsw_free_rejects_invalid_arguments)
{
	START;

	surface_driver_t *drv = t_surface_wsw_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->free(NULL), 1);
	EXPECT_EQ(drv->free(&(surface_t){0}), 1);

	END;
}

TEST(surface_wsw_unbind_rejects_invalid_arguments)
{
	START;

	surface_driver_t *drv = t_surface_wsw_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->unbind(NULL), 1);
	EXPECT_EQ(drv->unbind(&(surface_t){0}), 1);

	END;
}

TEST(surface_wsw_config_window_rejects_invalid_arguments)
{
	START;

	surface_driver_t *drv = t_surface_wsw_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->config_window(NULL, &(window_config_t){0}), 1);
	EXPECT_EQ(drv->config_window(&(surface_t){0}, &(window_config_t){0}), 1);

	t_surface_wsw_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_wsw_open(&surface), 0);
	EXPECT_EQ(surface_config_window(&surface, NULL), 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_config_window_rejects_missing_native_display)
{
	START;

	t_surface_wsw_reset();
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_wsw_open(&surface), 0);
	t_display_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_config_window(&surface, &config), 1);
	log_set_quiet(0, 0);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_config_window_sets_defaults)
{
	START;

	t_surface_wsw_reset();
	surface_t surface      = {0};
	window_config_t config = {.depth = 24, .visual = 1};
	EXPECT_EQ(t_surface_wsw_open(&surface), 0);

	EXPECT_EQ(surface_config_window(&surface, &config), 0);
	EXPECT_EQ(config.depth, 0);
	EXPECT_EQ(config.visual, 0);
	EXPECT_EQ(config.background, WINDOW_BACKGROUND_NONE);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_bind_rejects_invalid_arguments)
{
	START;

	surface_driver_t *drv = t_surface_wsw_driver();
	EXPECT_NOT_NULL(drv);
	EXPECT_EQ(drv->bind(NULL, &(window_t){0}), 1);
	EXPECT_EQ(drv->bind(&(surface_t){0}, &(window_t){0}), 1);

	t_surface_wsw_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_wsw_open(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, NULL), 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_bind_rejects_missing_native_display)
{
	START;

	t_surface_wsw_reset();
	surface_t surface = {0};
	window_t window	  = {.display = &t_display};
	EXPECT_EQ(t_surface_wsw_open(&surface), 0);
	t_display_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_bind_rejects_missing_window_native)
{
	START;

	t_surface_wsw_reset();
	t_window_native_ret = 1;
	surface_t surface   = {0};
	window_t window	    = {.display = &t_display};
	EXPECT_EQ(t_surface_wsw_open(&surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_bind_get_dc_failure)
{
	START;

	t_surface_wsw_reset();
	t_wsw_get_dc_ret  = NULL;
	surface_t surface = {0};
	window_t window	  = {.display = &t_display};
	EXPECT_EQ(t_surface_wsw_open(&surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_bind_replaces_existing_window)
{
	START;

	t_surface_wsw_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);
	t_wsw_release_dc_calls = 0;
	t_window_native_window = (void *)0x7777;
	window_t window	       = {.display = &t_display};

	EXPECT_EQ(surface_bind(&surface, &window), 0);
	EXPECT_EQ(t_wsw_release_dc_calls, 1);
	EXPECT_EQ(t_wsw_get_dc_calls, 2);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_bind_success)
{
	START;

	t_surface_wsw_reset();
	surface_t surface	= {0};
	surface_native_t native = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);

	EXPECT_EQ(surface_native(&surface, &native), 0);
	EXPECT_EQ(native.gfx_api, GFX_API_SOFTWARE);
	EXPECT_EQ(native.native_type, DISPLAY_NATIVE_WINDOWS);
	EXPECT_PTR(native.display, t_wsw_get_dc_ret);
	EXPECT_EQ(native.handle, (u64)(uintptr_t)t_window_native_window);
	EXPECT_NOT_NULL(native.gfx_surface);
	EXPECT_EQ(native.gfx_surface->api, GFX_API_SOFTWARE);
	EXPECT_EQ(native.gfx_surface->handle, (u64)(uintptr_t)t_window_native_window);
	EXPECT_EQ(t_wsw_get_dc_calls, 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_native_rejects_invalid_arguments)
{
	START;

	t_surface_wsw_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);
	surface_driver_t *drv = t_surface_wsw_driver();
	EXPECT_NOT_NULL(drv);

	EXPECT_EQ(drv->native(NULL, &(surface_native_t){0}), 1);
	EXPECT_EQ(drv->native(&surface, NULL), 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_native_rejects_unbound_surface)
{
	START;

	t_surface_wsw_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_wsw_open(&surface), 0);

	EXPECT_EQ(surface_native(&surface, &(surface_native_t){0}), 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_memory_exposes_bgra_dib_pixels)
{
	START;

	t_surface_wsw_reset();
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);

	EXPECT_EQ(t_surface_wsw_memory(&surface, &memory), 0);
	EXPECT_EQ(memory.format, GFX_FORMAT_BGRA8_UNORM);
	EXPECT_PTR(memory.data, t_wsw_bitmap_pixels);
	EXPECT_EQ(memory.stride, 8);
	EXPECT_EQ(t_wsw_create_compatible_dc_calls, 1);
	EXPECT_EQ(t_wsw_create_dib_section_calls, 1);
	EXPECT_EQ(t_wsw_select_object_calls, 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_memory_reuses_matching_bitmap)
{
	START;

	t_surface_wsw_reset();
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);
	EXPECT_EQ(t_surface_wsw_memory(&surface, &memory), 0);
	void *pixels = memory.data;

	EXPECT_EQ(t_surface_wsw_memory(&surface, &memory), 0);
	EXPECT_PTR(memory.data, pixels);
	EXPECT_EQ(t_wsw_create_dib_section_calls, 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_memory_rejects_invalid_arguments)
{
	START;

	t_surface_wsw_reset();
	surface_t surface	= {0};
	surface_native_t native = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->memory(NULL, &(gfx_surface_memory_t){.width = 2, .height = 2}), 1);
	EXPECT_EQ(native.gfx_surface->ops->memory(native.gfx_surface, NULL), 1);
	EXPECT_EQ(native.gfx_surface->ops->memory(native.gfx_surface, &(gfx_surface_memory_t){.width = 0, .height = 2}), 1);
	EXPECT_EQ(native.gfx_surface->ops->memory(native.gfx_surface, &(gfx_surface_memory_t){.width = 2, .height = 0}), 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_memory_rejects_unbound_surface)
{
	START;

	t_surface_wsw_reset();
	surface_t surface	= {0};
	surface_native_t native = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);
	EXPECT_EQ(surface_native(&surface, &native), 0);
	gfx_surface_t gfx_surface = *native.gfx_surface;
	EXPECT_EQ(surface_unbind(&surface), 0);

	EXPECT_EQ(gfx_surface.ops->memory(&gfx_surface, &(gfx_surface_memory_t){.width = 2, .height = 2}), 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_memory_create_dc_failure)
{
	START;

	t_surface_wsw_reset();
	t_wsw_create_compatible_dc_ret = NULL;
	surface_t surface	       = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(t_surface_wsw_memory(&surface, &(gfx_surface_memory_t){.width = 2, .height = 2}), 1);
	log_set_quiet(0, 0);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_memory_create_dib_section_failure)
{
	START;

	t_surface_wsw_reset();
	t_wsw_create_dib_section_ret = NULL;
	surface_t surface	     = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(t_surface_wsw_memory(&surface, &(gfx_surface_memory_t){.width = 2, .height = 2}), 1);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_wsw_delete_dc_calls, 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_memory_create_dib_section_null_bits)
{
	START;

	t_surface_wsw_reset();
	t_wsw_create_dib_section_null_bits = 1;
	surface_t surface		   = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(t_surface_wsw_memory(&surface, &(gfx_surface_memory_t){.width = 2, .height = 2}), 1);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_wsw_delete_dc_calls, 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_memory_select_object_failure)
{
	START;

	t_surface_wsw_reset();
	t_wsw_select_object_ret = NULL;
	surface_t surface	= {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(t_surface_wsw_memory(&surface, &(gfx_surface_memory_t){.width = 2, .height = 2}), 1);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_wsw_delete_object_calls, 1);
	EXPECT_EQ(t_wsw_delete_dc_calls, 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_present_blits_bgra_dib_pixels)
{
	START;

	t_surface_wsw_reset();
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	surface_native_t native	    = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);
	EXPECT_EQ(t_surface_wsw_memory(&surface, &memory), 0);
	u8 *pixels = memory.data;
	pixels[0]  = 0x11;
	pixels[1]  = 0x22;
	pixels[2]  = 0x33;
	pixels[3]  = 0x44;
	pixels[4]  = 0x55;
	pixels[5]  = 0x66;
	pixels[6]  = 0x77;
	pixels[7]  = 0x88;
	EXPECT_EQ(surface_native(&surface, &native), 0);
	EXPECT_PTR(memory.data, t_wsw_bitmap_pixels);

	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT), 0);
	EXPECT_EQ(t_wsw_bit_blt_calls, 1);
	EXPECT_EQ(t_wsw_bit_blt_width, 2);
	EXPECT_EQ(t_wsw_bit_blt_height, 2);
	EXPECT_EQ(t_wsw_bitmap_pixels[0], 0x11);
	EXPECT_EQ(t_wsw_bitmap_pixels[1], 0x22);
	EXPECT_EQ(t_wsw_bitmap_pixels[2], 0x33);
	EXPECT_EQ(t_wsw_bitmap_pixels[3], 0x44);
	EXPECT_EQ(t_wsw_bitmap_pixels[4], 0x55);
	EXPECT_EQ(t_wsw_bitmap_pixels[5], 0x66);
	EXPECT_EQ(t_wsw_bitmap_pixels[6], 0x77);
	EXPECT_EQ(t_wsw_bitmap_pixels[7], 0x88);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_present_rejects_invalid_arguments)
{
	START;

	t_surface_wsw_reset();
	surface_t surface	= {0};
	surface_native_t native = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->present(NULL, GFX_PRESENT_MODE_DEFAULT), 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_present_bit_blt_failure)
{
	START;

	t_surface_wsw_reset();
	t_wsw_bit_blt_ret	    = 0;
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	surface_native_t native	    = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);
	EXPECT_EQ(t_surface_wsw_memory(&surface, &memory), 0);
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT), 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_unbind_releases_gdi_objects)
{
	START;

	t_surface_wsw_reset();
	surface_t surface	    = {0};
	gfx_surface_memory_t memory = {.width = 2, .height = 2};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);
	EXPECT_EQ(t_surface_wsw_memory(&surface, &memory), 0);
	t_wsw_select_object_calls = 0;

	EXPECT_EQ(surface_unbind(&surface), 0);
	EXPECT_EQ(t_wsw_select_object_calls, 1);
	EXPECT_PTR(t_wsw_selected_object, t_wsw_select_object_ret);
	EXPECT_EQ(t_wsw_delete_object_calls, 1);
	EXPECT_EQ(t_wsw_delete_dc_calls, 1);
	EXPECT_EQ(t_wsw_release_dc_calls, 1);

	t_surface_wsw_close(&surface);
	END;
}

TEST(surface_wsw_present_rejects_unprepared_surface)
{
	START;

	t_surface_wsw_reset();
	surface_t surface	= {0};
	surface_native_t native = {0};
	EXPECT_EQ(t_surface_wsw_open_bound(&surface), 0);
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT), 1);

	t_surface_wsw_close(&surface);
	END;
}

STEST(surface_wsw)
{
	SSTART;

	RUN(surface_wsw_driver_is_registered);
	RUN(surface_wsw_compatible_accepts_windows_software);
	RUN(surface_wsw_compatible_rejects_invalid_info);
	RUN(surface_wsw_init_rejects_invalid_arguments);
	RUN(surface_wsw_init_alloc_failure);
	RUN(surface_wsw_init_rejects_missing_user32_library);
	RUN(surface_wsw_init_rejects_missing_gdi32_library);
	RUN(surface_wsw_init_rejects_missing_symbol);
	RUN(surface_wsw_free_rejects_invalid_arguments);
	RUN(surface_wsw_unbind_rejects_invalid_arguments);
	RUN(surface_wsw_config_window_rejects_invalid_arguments);
	RUN(surface_wsw_config_window_rejects_missing_native_display);
	RUN(surface_wsw_config_window_sets_defaults);
	RUN(surface_wsw_bind_rejects_invalid_arguments);
	RUN(surface_wsw_bind_rejects_missing_native_display);
	RUN(surface_wsw_bind_rejects_missing_window_native);
	RUN(surface_wsw_bind_get_dc_failure);
	RUN(surface_wsw_bind_replaces_existing_window);
	RUN(surface_wsw_bind_success);
	RUN(surface_wsw_native_rejects_invalid_arguments);
	RUN(surface_wsw_native_rejects_unbound_surface);
	RUN(surface_wsw_memory_exposes_bgra_dib_pixels);
	RUN(surface_wsw_memory_reuses_matching_bitmap);
	RUN(surface_wsw_memory_rejects_invalid_arguments);
	RUN(surface_wsw_memory_rejects_unbound_surface);
	RUN(surface_wsw_memory_create_dc_failure);
	RUN(surface_wsw_memory_create_dib_section_failure);
	RUN(surface_wsw_memory_create_dib_section_null_bits);
	RUN(surface_wsw_memory_select_object_failure);
	RUN(surface_wsw_present_blits_bgra_dib_pixels);
	RUN(surface_wsw_present_rejects_invalid_arguments);
	RUN(surface_wsw_present_bit_blt_failure);
	RUN(surface_wsw_unbind_releases_gdi_objects);
	RUN(surface_wsw_present_rejects_unprepared_surface);

	SEND;
}

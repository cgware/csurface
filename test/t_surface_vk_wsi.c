#include "surface_driver.h"

#include "display_driver.h"
#include "gfx_driver.h"
#include "log.h"
#include "test.h"

typedef void Display;
typedef unsigned long Window;
typedef void *HINSTANCE;
typedef void *HWND;
typedef u64 VkInstance;
typedef u64 VkSurfaceKHR;

enum {
	VK_SUCCESS = 0,
};

typedef struct VkXlibSurfaceCreateInfoKHR_s {
	u32 sType;
	const void *pNext;
	u32 flags;
	Display *dpy;
	Window window;
} VkXlibSurfaceCreateInfoKHR;

typedef struct VkWin32SurfaceCreateInfoKHR_s {
	u32 sType;
	const void *pNext;
	u32 flags;
	HINSTANCE hinstance;
	HWND hwnd;
} VkWin32SurfaceCreateInfoKHR;

typedef void (*t_surface_vk_wsi_symbol_t)(void);

static int t_vk_create_xlib_surface_calls;
static int t_vk_create_win32_surface_calls;
static int t_vk_destroy_surface_calls;
static int t_vk_create_xlib_surface_ret;
static int t_vk_create_win32_surface_ret;
static VkInstance t_vk_instance;
static VkSurfaceKHR t_vk_surface;
static VkSurfaceKHR t_vk_destroyed_surface;
static Display *t_vk_xlib_display;
static Window t_vk_xlib_window;
static HINSTANCE t_vk_win32_instance;
static HWND t_vk_win32_window;
static int t_gfx_native_ret;
static gfx_api_t t_gfx_native_api;
static u64 t_gfx_native_instance;
static int t_display_native_ret;
static display_native_type_t t_display_native_type;
static void *t_display_native_display;
static int t_window_native_ret;
static display_native_type_t t_window_native_type;
static void *t_window_native_window;

static void *t_surface_vk_wsi_alloc_fail(alloc_t *alloc, size_t size)
{
	(void)alloc;
	(void)size;
	return NULL;
}

static void *t_surface_vk_wsi_symbol(t_surface_vk_wsi_symbol_t fn)
{
	union {
		t_surface_vk_wsi_symbol_t fn;
		void *ptr;
	} symbol = {.fn = fn};

	return symbol.ptr;
}

static int t_vkCreateXlibSurfaceKHR(VkInstance instance, const VkXlibSurfaceCreateInfoKHR *create, const void *alloc, VkSurfaceKHR *surface)
{
	(void)alloc;
	t_vk_create_xlib_surface_calls++;
	t_vk_instance	  = instance;
	t_vk_xlib_display = create->dpy;
	t_vk_xlib_window  = create->window;
	*surface	  = t_vk_surface;
	return t_vk_create_xlib_surface_ret;
}

static int t_vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR *create, const void *alloc,
				     VkSurfaceKHR *surface)
{
	(void)alloc;
	t_vk_create_win32_surface_calls++;
	t_vk_instance	    = instance;
	t_vk_win32_instance = create->hinstance;
	t_vk_win32_window   = create->hwnd;
	*surface	    = t_vk_surface;
	return t_vk_create_win32_surface_ret;
}

static void t_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const void *alloc)
{
	(void)alloc;
	t_vk_destroy_surface_calls++;
	t_vk_instance	       = instance;
	t_vk_destroyed_surface = surface;
}

static int t_surface_vk_wsi_gfx_proc(gfx_t *gfx, strv_t name, void **sym)
{
	void *lib = NULL;
	if (proc_dlopen(gfx->data, STRV("libvulkan.so.1"), &lib)) {
		return 1;
	}

	return proc_dlsym(gfx->data, lib, name, sym);
}

static int t_surface_vk_wsi_gfx_native(gfx_t *gfx, gfx_native_t *native)
{
	(void)gfx;
	if (t_gfx_native_ret) {
		return t_gfx_native_ret;
	}
	*native = (gfx_native_t){
		.api	  = t_gfx_native_api,
		.instance = t_gfx_native_instance,
	};
	return 0;
}

static gfx_driver_t t_surface_vk_wsi_gfx_driver = {
	.name	= "test",
	.api	= GFX_API_VULKAN,
	.native = t_surface_vk_wsi_gfx_native,
	.proc	= t_surface_vk_wsi_gfx_proc,
};

static int t_surface_vk_wsi_display_native(display_t *display, display_native_t *native)
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

static int t_surface_vk_wsi_window_native(window_t *window, window_native_t *native)
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

static display_driver_t t_surface_vk_wsi_display_driver = {
	.name	       = "test",
	.native	       = t_surface_vk_wsi_display_native,
	.window_native = t_surface_vk_wsi_window_native,
};

static surface_driver_t *t_surface_vk_wsi_driver(void)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type == SURFACE_DRIVER_TYPE) {
			surface_driver_t *drv = i->data;
			if (strv_eq(strv_cstr(drv->name), STRV("vk_wsi"))) {
				return drv;
			}
		}
	}

	return NULL;
}

static void t_surface_vk_wsi_reset(void)
{
	t_vk_create_xlib_surface_calls	= 0;
	t_vk_create_win32_surface_calls = 0;
	t_vk_destroy_surface_calls	= 0;
	t_vk_create_xlib_surface_ret	= VK_SUCCESS;
	t_vk_create_win32_surface_ret	= VK_SUCCESS;
	t_vk_instance			= 0;
	t_vk_surface			= 0x12345678;
	t_vk_destroyed_surface		= 0;
	t_vk_xlib_display		= NULL;
	t_vk_xlib_window		= 0;
	t_vk_win32_instance		= NULL;
	t_vk_win32_window		= NULL;
	t_gfx_native_ret		= 0;
	t_gfx_native_api		= GFX_API_VULKAN;
	t_gfx_native_instance		= 0x9876;
	t_display_native_ret		= 0;
	t_display_native_type		= DISPLAY_NATIVE_X11;
	t_display_native_display	= (void *)0x1111;
	t_window_native_ret		= 0;
	t_window_native_type		= DISPLAY_NATIVE_X11;
	t_window_native_window		= (void *)(uintptr_t)0x2222;
}

static void t_surface_vk_wsi_symbols(proc_t *proc)
{
	proc_setdlsym(proc,
		      STRV("libvulkan.so.1"),
		      STRV("vkCreateXlibSurfaceKHR"),
		      t_surface_vk_wsi_symbol((t_surface_vk_wsi_symbol_t)t_vkCreateXlibSurfaceKHR));
	proc_setdlsym(proc,
		      STRV("libvulkan.so.1"),
		      STRV("vkCreateWin32SurfaceKHR"),
		      t_surface_vk_wsi_symbol((t_surface_vk_wsi_symbol_t)t_vkCreateWin32SurfaceKHR));
	proc_setdlsym(proc,
		      STRV("libvulkan.so.1"),
		      STRV("vkDestroySurfaceKHR"),
		      t_surface_vk_wsi_symbol((t_surface_vk_wsi_symbol_t)t_vkDestroySurfaceKHR));
}

static int t_surface_vk_wsi_open(proc_t *proc, gfx_t *gfx, display_t *display, surface_t *surface)
{
	proc_init(proc, 0, 1, ALLOC_STD);
	t_surface_vk_wsi_symbols(proc);
	*gfx = (gfx_t){
		.drv  = &t_surface_vk_wsi_gfx_driver,
		.data = proc,
	};
	*display = (display_t){
		.drv = &t_surface_vk_wsi_display_driver,
	};

	surface_config_t config = {
		.display = display,
		.gfx	 = gfx,
		.alloc	 = ALLOC_STD,
	};

	return surface_init(surface, &config) == NULL;
}

static void t_surface_vk_wsi_close(proc_t *proc, surface_t *surface)
{
	surface_free(surface);
	proc_free(proc);
}

TEST(surface_vk_wsi_driver_is_registered)
{
	START;

	EXPECT_NOT_NULL(t_surface_vk_wsi_driver());

	END;
}

TEST(surface_vk_wsi_plan_x11_extension_count)
{
	START;

	t_surface_vk_wsi_reset();
	display_t display   = {.drv = &t_surface_vk_wsi_display_driver};
	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &display, .gfx_api = GFX_API_VULKAN}), 0);
	EXPECT_EQ(plan.gfx.instance_extension_count, 2);

	END;
}

TEST(surface_vk_wsi_plan_x11_platform_extension)
{
	START;

	t_surface_vk_wsi_reset();
	display_t display   = {.drv = &t_surface_vk_wsi_display_driver};
	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &display, .gfx_api = GFX_API_VULKAN}), 0);
	EXPECT_EQ(t_strcmp(plan.gfx.instance_extensions[1], "VK_KHR_xlib_surface"), 0);

	END;
}

TEST(surface_vk_wsi_plan_has_no_device_extensions)
{
	START;

	t_surface_vk_wsi_reset();
	display_t display   = {.drv = &t_surface_vk_wsi_display_driver};
	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &display, .gfx_api = GFX_API_VULKAN}), 0);
	EXPECT_EQ(plan.gfx.device_extension_count, 0);

	END;
}

TEST(surface_vk_wsi_plan_windows_platform_extension)
{
	START;

	t_surface_vk_wsi_reset();
	t_display_native_type = DISPLAY_NATIVE_WINDOWS;
	display_t display     = {.drv = &t_surface_vk_wsi_display_driver};
	surface_plan_t plan   = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &display, .gfx_api = GFX_API_VULKAN}), 0);
	EXPECT_EQ(t_strcmp(plan.gfx.instance_extensions[1], "VK_KHR_win32_surface"), 0);

	END;
}

TEST(surface_vk_wsi_plan_rejects_non_vulkan)
{
	START;

	t_surface_vk_wsi_reset();
	surface_driver_t *drv = t_surface_vk_wsi_driver();
	EXPECT_NOT_NULL(drv);
	surface_plan_t plan = {0};

	EXPECT_EQ(drv->plan(&(surface_info_t){.gfx_api = GFX_API_OPENGL, .native_type = DISPLAY_NATIVE_X11}, &plan), 1);

	END;
}

TEST(surface_vk_wsi_plan_rejects_null_plan)
{
	START;

	t_surface_vk_wsi_reset();
	surface_driver_t *drv = t_surface_vk_wsi_driver();
	EXPECT_NOT_NULL(drv);

	EXPECT_EQ(drv->plan(&(surface_info_t){.gfx_api = GFX_API_VULKAN, .native_type = DISPLAY_NATIVE_X11}, NULL), 1);

	END;
}

TEST(surface_vk_wsi_init_rejects_non_vulkan)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);
	t_surface_vk_wsi_symbols(&proc);
	t_gfx_native_api  = GFX_API_OPENGL;
	gfx_t gfx	  = {.drv = &t_surface_vk_wsi_gfx_driver, .data = &proc};
	display_t display = {.drv = &t_surface_vk_wsi_display_driver};
	surface_t surface = {0};

	log_set_quiet(0, 1);
	surface_config_t config = {
		.display = &display,
		.gfx	 = &gfx,
		.alloc	 = ALLOC_STD,
	};

	EXPECT_NULL(surface_init(&surface, &config));
	log_set_quiet(0, 0);

	proc_free(&proc);
	END;
}

TEST(surface_vk_wsi_init_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_vk_wsi_driver();
	EXPECT_NOT_NULL(drv);

	surface_config_t config = {
		.alloc = ALLOC_STD,
	};

	EXPECT_EQ(drv->init(NULL, &config), 1);

	END;
}

TEST(surface_vk_wsi_init_alloc_failure)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);
	t_surface_vk_wsi_symbols(&proc);
	gfx_t gfx	  = {.drv = &t_surface_vk_wsi_gfx_driver, .data = &proc};
	display_t display = {.drv = &t_surface_vk_wsi_display_driver};
	surface_t surface = {0};

	surface_config_t config = {
		.display = &display,
		.gfx	 = &gfx,
		.alloc	 = {.alloc = t_surface_vk_wsi_alloc_fail},
	};

	log_set_quiet(0, 1);
	EXPECT_NULL(surface_init_driver(&surface, t_surface_vk_wsi_driver(), &config));
	log_set_quiet(0, 0);

	proc_free(&proc);
	END;
}

TEST(surface_vk_wsi_init_missing_gfx_native)
{
	START;

	t_surface_vk_wsi_reset();
	t_gfx_native_ret  = 1;
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);
	t_surface_vk_wsi_symbols(&proc);
	gfx	= (gfx_t){.drv = &t_surface_vk_wsi_gfx_driver, .data = &proc};
	display = (display_t){.drv = &t_surface_vk_wsi_display_driver};

	surface_config_t config = {
		.display = &display,
		.gfx	 = &gfx,
		.alloc	 = ALLOC_STD,
	};

	log_set_quiet(0, 1);
	EXPECT_NULL(surface_init_driver(&surface, t_surface_vk_wsi_driver(), &config));
	log_set_quiet(0, 0);

	proc_free(&proc);
	END;
}

TEST(surface_vk_wsi_init_missing_destroy_surface)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);
	gfx_t gfx	  = {.drv = &t_surface_vk_wsi_gfx_driver, .data = &proc};
	display_t display = {.drv = &t_surface_vk_wsi_display_driver};
	surface_t surface = {0};

	surface_config_t config = {
		.display = &display,
		.gfx	 = &gfx,
		.alloc	 = ALLOC_STD,
	};

	log_set_quiet(0, 1);
	EXPECT_NULL(surface_init_driver(&surface, t_surface_vk_wsi_driver(), &config));
	log_set_quiet(0, 0);

	proc_free(&proc);
	END;
}

TEST(surface_vk_wsi_unbind_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_vk_wsi_driver();
	EXPECT_NOT_NULL(drv);

	EXPECT_EQ(drv->unbind(NULL), 1);

	END;
}

TEST(surface_vk_wsi_free_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_vk_wsi_driver();
	EXPECT_NOT_NULL(drv);

	EXPECT_EQ(drv->free(NULL), 1);

	END;
}

TEST(surface_vk_wsi_config_window_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_vk_wsi_driver();
	EXPECT_NOT_NULL(drv);
	window_config_t config = {0};

	EXPECT_EQ(drv->config_window(NULL, &config), 1);

	END;
}

TEST(surface_vk_wsi_config_window_native_unavailable)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	t_display_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_config_window(&surface, &config), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_config_window_missing_display_handle)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	t_display_native_display = NULL;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_config_window(&surface, &config), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_config_window_sets_depth)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {.depth = 24};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);

	surface_config_window(&surface, &config);

	EXPECT_EQ(config.depth, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_config_window_sets_visual)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {.visual = 1234};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);

	surface_config_window(&surface, &config);

	EXPECT_EQ(config.visual, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_config_window_omits_background)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_config_t config = {0};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);

	surface_config_window(&surface, &config);

	EXPECT_EQ(config.background, WINDOW_BACKGROUND_NONE);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_x11_creates_surface)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	proc_init(&proc, 0, 1, ALLOC_STD);
	t_surface_vk_wsi_symbols(&proc);
	gfx			= (gfx_t){.drv = &t_surface_vk_wsi_gfx_driver, .data = &proc};
	display			= (display_t){.drv = &t_surface_vk_wsi_display_driver};
	surface_config_t config = {
		.display = &display,
		.gfx	 = &gfx,
		.alloc	 = ALLOC_STD,
	};
	EXPECT_NOT_NULL(surface_init_driver(&surface, t_surface_vk_wsi_driver(), &config));

	surface_bind(&surface, &window);

	EXPECT_EQ(t_vk_create_xlib_surface_calls, 1);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_x11_passes_window)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	proc_init(&proc, 0, 1, ALLOC_STD);
	t_surface_vk_wsi_symbols(&proc);
	gfx			= (gfx_t){.drv = &t_surface_vk_wsi_gfx_driver, .data = &proc};
	display			= (display_t){.drv = &t_surface_vk_wsi_display_driver};
	surface_config_t config = {
		.display = &display,
		.gfx	 = &gfx,
		.alloc	 = ALLOC_STD,
	};
	EXPECT_NOT_NULL(surface_init_driver(&surface, t_surface_vk_wsi_driver(), &config));

	surface_bind(&surface, &window);

	EXPECT_EQ(t_vk_xlib_window, 0x2222);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_windows_creates_surface)
{
	START;

	t_surface_vk_wsi_reset();
	t_display_native_type	 = DISPLAY_NATIVE_WINDOWS;
	t_window_native_type	 = DISPLAY_NATIVE_WINDOWS;
	t_display_native_display = (void *)0x3333;
	t_window_native_window	 = (void *)0x4444;
	proc_t proc		 = {0};
	gfx_t gfx		 = {0};
	display_t display	 = {0};
	surface_t surface	 = {0};
	window_t window		 = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);

	surface_bind(&surface, &window);

	EXPECT_EQ(t_vk_create_win32_surface_calls, 1);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_vk_wsi_driver();
	EXPECT_NOT_NULL(drv);
	window_t window = {0};

	EXPECT_EQ(drv->bind(NULL, &window), 1);

	END;
}

TEST(surface_vk_wsi_bind_display_unavailable)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	t_display_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_window_unavailable)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	t_window_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_x11_missing_display_handle)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	t_display_native_display = NULL;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_x11_missing_window_handle)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	t_window_native_window = NULL;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_x11_missing_create_symbol)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);
	proc_setdlsym(&proc,
		      STRV("libvulkan.so.1"),
		      STRV("vkDestroySurfaceKHR"),
		      t_surface_vk_wsi_symbol((t_surface_vk_wsi_symbol_t)t_vkDestroySurfaceKHR));
	gfx_t gfx		= {.drv = &t_surface_vk_wsi_gfx_driver, .data = &proc};
	display_t display	= {.drv = &t_surface_vk_wsi_display_driver};
	surface_t surface	= {0};
	window_t window		= {.display = &display};
	surface_config_t config = {
		.display = &display,
		.gfx	 = &gfx,
		.alloc	 = ALLOC_STD,
	};
	EXPECT_NOT_NULL(surface_init_driver(&surface, t_surface_vk_wsi_driver(), &config));

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_x11_create_failure)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	t_vk_create_xlib_surface_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_windows_missing_display_handle)
{
	START;

	t_surface_vk_wsi_reset();
	t_display_native_type	 = DISPLAY_NATIVE_WINDOWS;
	t_window_native_type	 = DISPLAY_NATIVE_WINDOWS;
	t_display_native_display = NULL;
	proc_t proc		 = {0};
	gfx_t gfx		 = {0};
	display_t display	 = {0};
	surface_t surface	 = {0};
	window_t window		 = {.display = &display};
	proc_init(&proc, 0, 1, ALLOC_STD);
	t_surface_vk_wsi_symbols(&proc);
	gfx			= (gfx_t){.drv = &t_surface_vk_wsi_gfx_driver, .data = &proc};
	display			= (display_t){.drv = &t_surface_vk_wsi_display_driver};
	surface_config_t config = {
		.display = &display,
		.gfx	 = &gfx,
		.alloc	 = ALLOC_STD,
	};
	EXPECT_NOT_NULL(surface_init_driver(&surface, t_surface_vk_wsi_driver(), &config));

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_windows_missing_window_handle)
{
	START;

	t_surface_vk_wsi_reset();
	t_display_native_type  = DISPLAY_NATIVE_WINDOWS;
	t_window_native_type   = DISPLAY_NATIVE_WINDOWS;
	t_window_native_window = NULL;
	proc_t proc	       = {0};
	gfx_t gfx	       = {0};
	display_t display      = {0};
	surface_t surface      = {0};
	window_t window	       = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_windows_missing_create_symbol)
{
	START;

	t_surface_vk_wsi_reset();
	t_display_native_type = DISPLAY_NATIVE_WINDOWS;
	t_window_native_type  = DISPLAY_NATIVE_WINDOWS;
	proc_t proc	      = {0};
	proc_init(&proc, 0, 1, ALLOC_STD);
	proc_setdlsym(&proc,
		      STRV("libvulkan.so.1"),
		      STRV("vkDestroySurfaceKHR"),
		      t_surface_vk_wsi_symbol((t_surface_vk_wsi_symbol_t)t_vkDestroySurfaceKHR));
	gfx_t gfx		= {.drv = &t_surface_vk_wsi_gfx_driver, .data = &proc};
	display_t display	= {.drv = &t_surface_vk_wsi_display_driver};
	surface_t surface	= {0};
	window_t window		= {.display = &display};
	surface_config_t config = {
		.display = &display,
		.gfx	 = &gfx,
		.alloc	 = ALLOC_STD,
	};
	EXPECT_NOT_NULL(surface_init_driver(&surface, t_surface_vk_wsi_driver(), &config));

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_windows_create_failure)
{
	START;

	t_surface_vk_wsi_reset();
	t_display_native_type = DISPLAY_NATIVE_WINDOWS;
	t_window_native_type  = DISPLAY_NATIVE_WINDOWS;
	proc_t proc	      = {0};
	gfx_t gfx	      = {0};
	display_t display     = {0};
	surface_t surface     = {0};
	window_t window	      = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	t_vk_create_win32_surface_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_unsupported_native_type)
{
	START;

	t_surface_vk_wsi_reset();
	t_display_native_type = DISPLAY_NATIVE_NONE;
	t_window_native_type  = DISPLAY_NATIVE_NONE;
	proc_t proc	      = {0};
	gfx_t gfx	      = {0};
	display_t display     = {0};
	surface_t surface     = {0};
	window_t window	      = {.display = &display};
	proc_init(&proc, 0, 1, ALLOC_STD);
	t_surface_vk_wsi_symbols(&proc);
	gfx			= (gfx_t){.drv = &t_surface_vk_wsi_gfx_driver, .data = &proc};
	display			= (display_t){.drv = &t_surface_vk_wsi_display_driver};
	surface_config_t config = {
		.display = &display,
		.gfx	 = &gfx,
		.alloc	 = ALLOC_STD,
	};
	EXPECT_NOT_NULL(surface_init_driver(&surface, t_surface_vk_wsi_driver(), &config));

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	log_set_quiet(0, 0);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_bind_replaces_surface)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	surface_bind(&surface, &window);

	EXPECT_EQ(surface_bind(&surface, &window), 0);
	EXPECT_EQ(t_vk_destroy_surface_calls, 1);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_unbind_destroys_surface)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	surface_bind(&surface, &window);

	surface_unbind(&surface);

	EXPECT_EQ(t_vk_destroyed_surface, 0x12345678);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_native_returns_handle)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	surface_bind(&surface, &window);

	surface_native_t native = {0};
	surface_native(&surface, &native);

	EXPECT_EQ(native.handle, 0x12345678);
	EXPECT_NOT_NULL(native.gfx_surface);
	EXPECT_EQ(native.gfx_surface->api, GFX_API_VULKAN);
	EXPECT_EQ(native.gfx_surface->handle, 0x12345678);
	EXPECT_NOT_NULL(native.gfx_surface->data);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_native_without_bind)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);

	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 1);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_native_null_native)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);

	EXPECT_EQ(surface.drv->native(&surface, NULL), 1);

	t_surface_vk_wsi_close(&proc, &surface);
	END;
}

TEST(surface_vk_wsi_free_destroys_surface)
{
	START;

	t_surface_vk_wsi_reset();
	proc_t proc	  = {0};
	gfx_t gfx	  = {0};
	display_t display = {0};
	surface_t surface = {0};
	window_t window	  = {.display = &display};
	EXPECT_EQ(t_surface_vk_wsi_open(&proc, &gfx, &display, &surface), 0);
	surface_bind(&surface, &window);

	surface_free(&surface);

	EXPECT_EQ(t_vk_destroy_surface_calls, 1);

	proc_free(&proc);
	END;
}

STEST(surface_vk_wsi)
{
	SSTART;

	RUN(surface_vk_wsi_driver_is_registered);
	RUN(surface_vk_wsi_plan_x11_extension_count);
	RUN(surface_vk_wsi_plan_x11_platform_extension);
	RUN(surface_vk_wsi_plan_has_no_device_extensions);
	RUN(surface_vk_wsi_plan_windows_platform_extension);
	RUN(surface_vk_wsi_plan_rejects_non_vulkan);
	RUN(surface_vk_wsi_plan_rejects_null_plan);
	RUN(surface_vk_wsi_init_rejects_non_vulkan);
	RUN(surface_vk_wsi_init_null_surface);
	RUN(surface_vk_wsi_init_alloc_failure);
	RUN(surface_vk_wsi_init_missing_gfx_native);
	RUN(surface_vk_wsi_init_missing_destroy_surface);
	RUN(surface_vk_wsi_unbind_null_surface);
	RUN(surface_vk_wsi_free_null_surface);
	RUN(surface_vk_wsi_config_window_null_surface);
	RUN(surface_vk_wsi_config_window_native_unavailable);
	RUN(surface_vk_wsi_config_window_missing_display_handle);
	RUN(surface_vk_wsi_config_window_sets_depth);
	RUN(surface_vk_wsi_config_window_sets_visual);
	RUN(surface_vk_wsi_config_window_omits_background);
	RUN(surface_vk_wsi_bind_x11_creates_surface);
	RUN(surface_vk_wsi_bind_x11_passes_window);
	RUN(surface_vk_wsi_bind_windows_creates_surface);
	RUN(surface_vk_wsi_bind_null_surface);
	RUN(surface_vk_wsi_bind_display_unavailable);
	RUN(surface_vk_wsi_bind_window_unavailable);
	RUN(surface_vk_wsi_bind_x11_missing_display_handle);
	RUN(surface_vk_wsi_bind_x11_missing_window_handle);
	RUN(surface_vk_wsi_bind_x11_missing_create_symbol);
	RUN(surface_vk_wsi_bind_x11_create_failure);
	RUN(surface_vk_wsi_bind_windows_missing_display_handle);
	RUN(surface_vk_wsi_bind_windows_missing_window_handle);
	RUN(surface_vk_wsi_bind_windows_missing_create_symbol);
	RUN(surface_vk_wsi_bind_windows_create_failure);
	RUN(surface_vk_wsi_bind_unsupported_native_type);
	RUN(surface_vk_wsi_bind_replaces_surface);
	RUN(surface_vk_wsi_unbind_destroys_surface);
	RUN(surface_vk_wsi_native_returns_handle);
	RUN(surface_vk_wsi_native_without_bind);
	RUN(surface_vk_wsi_native_null_native);
	RUN(surface_vk_wsi_free_destroys_surface);

	SEND;
}

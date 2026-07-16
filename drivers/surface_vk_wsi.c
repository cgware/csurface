#include "surface_driver.h"

#include "log.h"
#include "mem.h"

typedef void Display;
typedef unsigned long Window;
typedef void *HINSTANCE;
typedef void *HWND;
typedef u64 VkInstance;
typedef u64 VkSurfaceKHR;
typedef u32 VkFlags;

typedef enum VkResult_e {
	VK_SUCCESS = 0,
} VkResult;

enum {
	VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR	= 1000004000,
	VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR = 1000009000,
};

typedef struct VkXlibSurfaceCreateInfoKHR_s {
	u32 sType;
	const void *pNext;
	VkFlags flags;
	Display *dpy;
	Window window;
} VkXlibSurfaceCreateInfoKHR;

typedef struct VkWin32SurfaceCreateInfoKHR_s {
	u32 sType;
	const void *pNext;
	VkFlags flags;
	HINSTANCE hinstance;
	HWND hwnd;
} VkWin32SurfaceCreateInfoKHR;

typedef VkResult (*PFN_vkCreateXlibSurfaceKHR)(VkInstance, const VkXlibSurfaceCreateInfoKHR *, const void *, VkSurfaceKHR *);
typedef VkResult (*PFN_vkCreateWin32SurfaceKHR)(VkInstance, const VkWin32SurfaceCreateInfoKHR *, const void *, VkSurfaceKHR *);
typedef void (*PFN_vkDestroySurfaceKHR)(VkInstance, VkSurfaceKHR, const void *);

typedef struct surface_vk_wsi_s {
	VkInstance instance;
	VkSurfaceKHR surface;
	display_native_type_t native_type;
	PFN_vkCreateXlibSurfaceKHR CreateXlibSurfaceKHR;
	PFN_vkCreateWin32SurfaceKHR CreateWin32SurfaceKHR;
	PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
} surface_vk_wsi_t;

static int vk_ok(VkResult result)
{
	return result == VK_SUCCESS;
}

static int surface_vk_wsi_load_symbol(gfx_t *gfx, void **sym, strv_t name)
{
	if (gfx_proc(gfx, name, sym)) {
		log_error("csurface", "csurface_vk_wsi", NULL, "failed to load Vulkan symbol: %.*s", name.len, name.data);
		return 1;
	}

	return 0;
}

#define LOAD_VK(_gfx, _ctx, _name) surface_vk_wsi_load_symbol((_gfx), (void **)&(_ctx)->_name, STRV("vk" #_name))

static int surface_vk_wsi_compatible(const surface_info_t *info)
{
	return info != NULL && info->gfx_api == GFX_API_VULKAN &&
	       (info->native_type == DISPLAY_NATIVE_X11 || info->native_type == DISPLAY_NATIVE_WINDOWS);
}

static int surface_vk_wsi_plan(const surface_info_t *info, surface_plan_t *plan)
{
	static const char *const x11_extensions[] = {
		"VK_KHR_surface",
		"VK_KHR_xlib_surface",
	};
	static const char *const windows_extensions[] = {
		"VK_KHR_surface",
		"VK_KHR_win32_surface",
	};

	if (!surface_vk_wsi_compatible(info) || plan == NULL) {
		return 1;
	}

	if (info->native_type == DISPLAY_NATIVE_X11) {
		plan->gfx.instance_extensions	   = x11_extensions;
		plan->gfx.instance_extension_count = sizeof(x11_extensions) / sizeof(x11_extensions[0]);
		return 0;
	}
	plan->gfx.instance_extensions	   = windows_extensions;
	plan->gfx.instance_extension_count = sizeof(windows_extensions) / sizeof(windows_extensions[0]);
	return 0;
}

static int surface_vk_wsi_init(surface_t *srf, const surface_config_t *config)
{
	if (srf == NULL || config == NULL || config->gfx == NULL || config->alloc.alloc == NULL) {
		return 1;
	}

	gfx_native_t native_gfx = {0};
	if (gfx_native(config->gfx, &native_gfx) || native_gfx.api != GFX_API_VULKAN || native_gfx.instance == 0) {
		log_error("csurface", "csurface_vk_wsi", NULL, "Vulkan native gfx instance is unavailable");
		return 1;
	}

	alloc_t alloc	      = config->alloc;
	surface_vk_wsi_t *ctx = alloc_alloc(&alloc, sizeof(*ctx));
	if (ctx == NULL) {
		log_error("csurface", "csurface_vk_wsi", NULL, "failed to allocate surface data");
		return 1;
	}
	mem_set(ctx, 0, sizeof(*ctx));
	ctx->instance = native_gfx.instance;

	if (LOAD_VK(config->gfx, ctx, DestroySurfaceKHR)) {
		alloc_free(&alloc, ctx, sizeof(*ctx));
		return 1;
	}

	srf->data = ctx;
	return 0;
}

static int surface_vk_wsi_unbind(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_vk_wsi_t *ctx = srf->data;
	if (ctx->surface != 0) {
		ctx->DestroySurfaceKHR(ctx->instance, ctx->surface, NULL);
		ctx->surface = 0;
	}
	ctx->native_type = DISPLAY_NATIVE_NONE;
	return 0;
}

static int surface_vk_wsi_free(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_vk_wsi_t *ctx = srf->data;
	surface_vk_wsi_unbind(srf);
	alloc_free(&srf->config.alloc, ctx, sizeof(*ctx));
	srf->data = NULL;
	return 0;
}

static int surface_vk_wsi_config_window(surface_t *srf, window_config_t *config)
{
	if (srf == NULL || srf->data == NULL || config == NULL) {
		return 1;
	}

	display_native_t native = {0};
	if (display_native(srf->config.display, &native) || (native.type != DISPLAY_NATIVE_X11 && native.type != DISPLAY_NATIVE_WINDOWS) ||
	    native.display == NULL) {
		log_error("csurface", "csurface_vk_wsi", NULL, "native display is unavailable");
		return 1;
	}

	config->depth	   = 0;
	config->visual	   = 0;
	config->background = WINDOW_BACKGROUND_NONE;
	return 0;
}

static int surface_vk_wsi_bind_x11(surface_t *srf, surface_vk_wsi_t *ctx, const display_native_t *display, const window_native_t *window)
{
	if (display->display == NULL || window->window == NULL) {
		log_error("csurface", "csurface_vk_wsi", NULL, "X11 native window is unavailable");
		return 1;
	}
	if (ctx->CreateXlibSurfaceKHR == NULL && LOAD_VK(srf->config.gfx, ctx, CreateXlibSurfaceKHR)) {
		return 1;
	}

	VkXlibSurfaceCreateInfoKHR create = {
		.sType	= VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		.dpy	= display->display,
		.window = (Window)(uintptr_t)window->window,
	};
	if (!vk_ok(ctx->CreateXlibSurfaceKHR(ctx->instance, &create, NULL, &ctx->surface))) {
		log_error("csurface", "csurface_vk_wsi", NULL, "failed to create Xlib Vulkan surface");
		return 1;
	}

	ctx->native_type = DISPLAY_NATIVE_X11;
	return 0;
}

static int surface_vk_wsi_bind_windows(surface_t *srf, surface_vk_wsi_t *ctx, const display_native_t *display,
				       const window_native_t *window)
{
	if (display->display == NULL || window->window == NULL) {
		log_error("csurface", "csurface_vk_wsi", NULL, "Windows native window is unavailable");
		return 1;
	}
	if (ctx->CreateWin32SurfaceKHR == NULL && LOAD_VK(srf->config.gfx, ctx, CreateWin32SurfaceKHR)) {
		return 1;
	}

	VkWin32SurfaceCreateInfoKHR create = {
		.sType	   = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = display->display,
		.hwnd	   = window->window,
	};
	if (!vk_ok(ctx->CreateWin32SurfaceKHR(ctx->instance, &create, NULL, &ctx->surface))) {
		log_error("csurface", "csurface_vk_wsi", NULL, "failed to create Win32 Vulkan surface");
		return 1;
	}

	ctx->native_type = DISPLAY_NATIVE_WINDOWS;
	return 0;
}

static int surface_vk_wsi_bind(surface_t *srf, window_t *window)
{
	if (srf == NULL || srf->data == NULL || window == NULL) {
		return 1;
	}

	display_native_t native_display = {0};
	if (display_native(srf->config.display, &native_display)) {
		log_error("csurface", "csurface_vk_wsi", NULL, "native display is unavailable");
		return 1;
	}

	window_native_t native_window = {0};
	if (window_native(window, &native_window) || native_window.type != native_display.type) {
		log_error("csurface", "csurface_vk_wsi", NULL, "native window is unavailable");
		return 1;
	}

	surface_vk_wsi_t *ctx = srf->data;
	if (ctx->surface != 0) {
		surface_vk_wsi_unbind(srf);
	}

	if (native_display.type == DISPLAY_NATIVE_X11) {
		return surface_vk_wsi_bind_x11(srf, ctx, &native_display, &native_window);
	}
	if (native_display.type == DISPLAY_NATIVE_WINDOWS) {
		return surface_vk_wsi_bind_windows(srf, ctx, &native_display, &native_window);
	}

	log_error("csurface", "csurface_vk_wsi", NULL, "unsupported native display");
	return 1;
}

static int surface_vk_wsi_native(surface_t *srf, surface_native_t *native)
{
	if (srf == NULL || srf->data == NULL || native == NULL) {
		return 1;
	}

	surface_vk_wsi_t *ctx = srf->data;
	if (ctx->surface == 0) {
		return 1;
	}

	*native = (surface_native_t){
		.gfx_api     = GFX_API_VULKAN,
		.native_type = ctx->native_type,
		.handle	     = ctx->surface,
	};
	return 0;
}

static surface_driver_t surface_vk_wsi = {
	.name	       = "vk_wsi",
	.compatible    = surface_vk_wsi_compatible,
	.plan	       = surface_vk_wsi_plan,
	.init	       = surface_vk_wsi_init,
	.free	       = surface_vk_wsi_free,
	.config_window = surface_vk_wsi_config_window,
	.bind	       = surface_vk_wsi_bind,
	.unbind	       = surface_vk_wsi_unbind,
	.native	       = surface_vk_wsi_native,
};

SURFACE_DRIVER(surface_vk_wsi, &surface_vk_wsi);

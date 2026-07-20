#include "surface_driver.h"

#include "display_driver.h"
#include "gfx_driver.h"
#include "log.h"
#include "test.h"

typedef long HRESULT;
typedef unsigned int UINT;
typedef unsigned long ULONG;
typedef void *HWND;
typedef void *IDXGIFactory;
typedef void *IDXGISwapChain;

enum {
	S_OK = 0,
};

typedef struct GUID_s {
	u32 Data1;
	u16 Data2;
	u16 Data3;
	u8 Data4[8];
} GUID;

typedef const GUID *REFIID;
typedef void (*t_surface_d3d11_symbol_t)(void);

typedef struct t_factory_vtbl_s {
	HRESULT (*QueryInterface)(void);
	ULONG (*AddRef)(void);
	ULONG (*Release)(IDXGIFactory *self);
	HRESULT (*SetPrivateData)(void);
	HRESULT (*SetPrivateDataInterface)(void);
	HRESULT (*GetPrivateData)(void);
	HRESULT (*GetParent)(void);
	HRESULT (*EnumAdapters)(void);
	HRESULT (*MakeWindowAssociation)(void);
	HRESULT (*GetWindowAssociation)(void);
	HRESULT (*CreateSwapChain)(IDXGIFactory *self, void *device, void *desc, IDXGISwapChain **swapchain);
} t_factory_vtbl_t;

typedef struct t_swapchain_vtbl_s {
	HRESULT (*QueryInterface)(void);
	ULONG (*AddRef)(void);
	ULONG (*Release)(IDXGISwapChain *self);
	HRESULT (*SetPrivateData)(void);
	HRESULT (*SetPrivateDataInterface)(void);
	HRESULT (*GetPrivateData)(void);
	HRESULT (*GetParent)(void);
	HRESULT (*GetDevice)(void);
	HRESULT (*Present)(IDXGISwapChain *self, UINT sync_interval, UINT flags);
} t_swapchain_vtbl_t;

typedef struct t_factory_s {
	t_factory_vtbl_t *vtbl;
} t_factory_t;

typedef struct t_swapchain_s {
	t_swapchain_vtbl_t *vtbl;
} t_swapchain_t;

static int t_create_factory_calls;
static int t_create_swapchain_calls;
static int t_present_calls;
static int t_release_factory_calls;
static int t_release_swapchain_calls;
static void *t_create_swapchain_device;
static int t_display_native_ret;
static int t_window_native_ret;
static int t_gfx_native_ret;
static display_native_type_t t_display_native_type;
static display_native_type_t t_window_native_type;
static HWND t_window_native_window;
static gfx_api_t t_gfx_native_api;
static u64 t_gfx_native_device;
static HRESULT t_create_factory_ret;
static int t_create_factory_null;
static HRESULT t_create_swapchain_ret;
static int t_create_swapchain_null;
static HRESULT t_present_ret;
static t_factory_t t_factory;
static t_swapchain_t t_swapchain;
static proc_t t_proc;
static display_t t_display;
static gfx_t t_gfx;
static window_t t_window;

static void *t_surface_d3d11_alloc_fail(alloc_t *alloc, size_t size)
{
	(void)alloc;
	(void)size;
	return NULL;
}

static void *t_surface_d3d11_symbol(t_surface_d3d11_symbol_t fn)
{
	union {
		t_surface_d3d11_symbol_t fn;
		void *ptr;
	} symbol = {.fn = fn};

	return symbol.ptr;
}

static ULONG t_factory_release(IDXGIFactory *self)
{
	(void)self;
	t_release_factory_calls++;
	return 0;
}

static ULONG t_swapchain_release(IDXGISwapChain *self)
{
	(void)self;
	t_release_swapchain_calls++;
	return 0;
}

static HRESULT t_CreateSwapChain(IDXGIFactory *self, void *device, void *desc, IDXGISwapChain **swapchain)
{
	(void)self;
	(void)desc;
	t_create_swapchain_calls++;
	t_create_swapchain_device = device;
	*swapchain		  = t_create_swapchain_null ? NULL : (IDXGISwapChain *)&t_swapchain;
	return t_create_swapchain_ret;
}

static HRESULT t_Present(IDXGISwapChain *self, UINT sync_interval, UINT flags)
{
	(void)self;
	(void)sync_interval;
	(void)flags;
	t_present_calls++;
	return t_present_ret;
}

static HRESULT t_CreateDXGIFactory(REFIID riid, void **factory)
{
	(void)riid;
	t_create_factory_calls++;
	*factory = t_create_factory_null ? NULL : &t_factory;
	return t_create_factory_ret;
}

static t_factory_vtbl_t t_factory_vtbl = {
	.Release	 = t_factory_release,
	.CreateSwapChain = t_CreateSwapChain,
};

static t_swapchain_vtbl_t t_swapchain_vtbl = {
	.Release = t_swapchain_release,
	.Present = t_Present,
};

static int t_display_native(display_t *display, display_native_t *native)
{
	(void)display;
	*native = (display_native_t){
		.type	 = t_display_native_type,
		.display = (void *)0x1234,
	};
	return t_display_native_ret;
}

static int t_window_native(window_t *window, window_native_t *native)
{
	(void)window;
	*native = (window_native_t){
		.type	= t_window_native_type,
		.window = t_window_native_window,
	};
	return t_window_native_ret;
}

static int t_gfx_native(gfx_t *gfx, gfx_native_t *native)
{
	(void)gfx;
	*native = (gfx_native_t){
		.api	= t_gfx_native_api,
		.device = t_gfx_native_device,
	};
	return t_gfx_native_ret;
}

static display_driver_t t_display_driver = {
	.name	       = "test",
	.native	       = t_display_native,
	.window_native = t_window_native,
};

static gfx_driver_t t_gfx_driver = {
	.name	= "test",
	.api	= GFX_API_D3D11,
	.native = t_gfx_native,
};

static void t_surface_d3d11_reset(void)
{
	t_create_factory_calls	  = 0;
	t_create_swapchain_calls  = 0;
	t_present_calls		  = 0;
	t_release_factory_calls	  = 0;
	t_release_swapchain_calls = 0;
	t_create_swapchain_device = NULL;
	t_display_native_ret	  = 0;
	t_window_native_ret	  = 0;
	t_gfx_native_ret	  = 0;
	t_display_native_type	  = DISPLAY_NATIVE_WINDOWS;
	t_window_native_type	  = DISPLAY_NATIVE_WINDOWS;
	t_window_native_window	  = (HWND)0x5678;
	t_gfx_native_api	  = GFX_API_D3D11;
	t_gfx_native_device	  = 0x9876;
	t_create_factory_ret	  = S_OK;
	t_create_factory_null	  = 0;
	t_create_swapchain_ret	  = S_OK;
	t_create_swapchain_null	  = 0;
	t_present_ret		  = S_OK;
	t_factory.vtbl		  = &t_factory_vtbl;
	t_swapchain.vtbl	  = &t_swapchain_vtbl;
	t_proc			  = (proc_t){0};
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	proc_setdlsym(&t_proc,
		      STRV("dxgi.dll"),
		      STRV("CreateDXGIFactory"),
		      t_surface_d3d11_symbol((t_surface_d3d11_symbol_t)t_CreateDXGIFactory));
	t_display = (display_t){
		.drv  = &t_display_driver,
		.proc = &t_proc,
	};
	t_gfx = (gfx_t){
		.drv = &t_gfx_driver,
	};
	t_window = (window_t){
		.display = &t_display,
	};
}

static void t_surface_d3d11_cleanup(void)
{
	proc_free(&t_proc);
}

static surface_driver_t *t_surface_d3d11_driver(void)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != SURFACE_DRIVER_TYPE) {
			continue;
		}

		surface_driver_t *drv = i->data;
		if (drv != NULL && strv_eq(strv_cstr(drv->name), STRV("d3d11"))) {
			return drv;
		}
	}

	return NULL;
}

static int t_surface_d3d11_init_surface(surface_t *surface)
{
	surface_config_t config = {
		.display = &t_display,
		.gfx	 = &t_gfx,
		.alloc	 = ALLOC_STD,
	};

	return surface_init(surface, &config) != surface;
}

TEST(surface_d3d11_plan_accepts_windows)
{
	START;

	t_surface_d3d11_reset();
	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &t_display, .gfx_api = GFX_API_D3D11}), 0);

	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_plan_rejects_non_windows)
{
	START;

	t_surface_d3d11_reset();
	t_display_native_type = DISPLAY_NATIVE_X11;
	surface_plan_t plan   = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &t_display, .gfx_api = GFX_API_D3D11}), 1);

	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_init_null_config)
{
	START;

	surface_driver_t *drv = t_surface_d3d11_driver();
	EXPECT_NOT_NULL(drv);

	EXPECT_EQ(drv->init(&(surface_t){0}, NULL), 1);

	END;
}

TEST(surface_d3d11_init_alloc_failure)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};

	surface_config_t config = {
		.display = &t_display,
		.gfx	 = &t_gfx,
		.alloc	 = {.alloc = t_surface_d3d11_alloc_fail},
	};

	log_set_quiet(0, 1);
	EXPECT_NULL(surface_init(&surface, &config));
	log_set_quiet(0, 0);

	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_unbind_null_data)
{
	START;

	surface_driver_t *drv = t_surface_d3d11_driver();
	EXPECT_NOT_NULL(drv);
	surface_t surface = {
		.drv = drv,
	};

	EXPECT_EQ(drv->unbind(&surface), 1);

	END;
}

TEST(surface_d3d11_free_null_data)
{
	START;

	surface_driver_t *drv = t_surface_d3d11_driver();
	EXPECT_NOT_NULL(drv);
	surface_t surface = {
		.drv = drv,
	};

	EXPECT_EQ(drv->free(&surface), 1);

	END;
}

TEST(surface_d3d11_config_window_null_data)
{
	START;

	surface_driver_t *drv = t_surface_d3d11_driver();
	EXPECT_NOT_NULL(drv);
	surface_t surface = {
		.drv = drv,
	};
	window_config_t config = {0};

	EXPECT_EQ(drv->config_window(&surface, &config), 1);

	END;
}

TEST(surface_d3d11_config_window_sets_background)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	window_config_t config = {
		.background = WINDOW_BACKGROUND_DEFAULT,
	};

	EXPECT_EQ(surface_config_window(&surface, &config), 0);
	EXPECT_EQ(config.background, WINDOW_BACKGROUND_NONE);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_null_data)
{
	START;

	surface_driver_t *drv = t_surface_d3d11_driver();
	EXPECT_NOT_NULL(drv);
	surface_t surface = {
		.drv = drv,
	};

	EXPECT_EQ(drv->bind(&surface, &t_window), 1);

	END;
}

TEST(surface_d3d11_bind_missing_gfx_native)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_gfx_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_rejects_non_d3d11_gfx)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_gfx_native_api = GFX_API_OPENGL;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_rejects_null_device)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_gfx_native_device = 0;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_missing_window_native)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_window_native_ret = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_rejects_non_windows_window)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_window_native_type = DISPLAY_NATIVE_X11;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_rejects_null_window_handle)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_window_native_window = NULL;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_missing_library)
{
	START;

	t_surface_d3d11_reset();
	proc_free(&t_proc);
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_missing_factory_symbol)
{
	START;

	t_surface_d3d11_reset();
	proc_free(&t_proc);
	proc_init(&t_proc, 0, 1, ALLOC_STD);
	proc_setdlsym(&t_proc, STRV("dxgi.dll"), STRV("unused"), &t_factory);
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_create_factory_failure)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_create_factory_ret = -1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_create_factory_null)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_create_factory_null = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_create_swapchain_failure)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_create_swapchain_ret = -1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_create_swapchain_null)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_create_swapchain_null = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_creates_swapchain)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);

	surface_bind(&surface, &t_window);

	EXPECT_EQ(t_create_swapchain_calls, 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_replaces_swapchain)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);

	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	EXPECT_EQ(t_release_swapchain_calls, 1);
	EXPECT_EQ(t_release_factory_calls, 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_passes_device)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);

	surface_bind(&surface, &t_window);

	EXPECT_PTR(t_create_swapchain_device, (void *)(uintptr_t)0x9876);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_unbind_releases_swapchain)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);

	EXPECT_EQ(surface_unbind(&surface), 0);
	EXPECT_EQ(t_release_swapchain_calls, 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_unbind_releases_factory)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);

	EXPECT_EQ(surface_unbind(&surface), 0);
	EXPECT_EQ(t_release_factory_calls, 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_native_sets_api)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	surface_bind(&surface, &t_window);
	surface_native_t native = {0};
	surface_native(&surface, &native);

	EXPECT_EQ(native.gfx_surface->api, GFX_API_D3D11);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_native_returns_ops)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_NOT_NULL(native.gfx_surface->ops);
	EXPECT_EQ(native.gfx_surface->ops->present == NULL, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_native_without_bind)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	surface_native_t native = {0};

	EXPECT_EQ(surface_native(&surface, &native), 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_native_null_data)
{
	START;

	surface_driver_t *drv = t_surface_d3d11_driver();
	EXPECT_NOT_NULL(drv);
	surface_native_t native = {0};

	surface_t surface = {
		.drv = drv,
	};

	EXPECT_EQ(drv->native(&surface, &native), 1);

	END;
}

TEST(surface_d3d11_gfx_present_calls_swapchain)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	surface_bind(&surface, &t_window);
	surface_native_t native = {0};
	surface_native(&surface, &native);

	native.gfx_surface->ops->present(native.gfx_surface);

	EXPECT_EQ(t_present_calls, 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_present_null_surface)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->present(NULL), 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_present_failure)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);
	t_present_ret = -1;

	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface), 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

STEST(surface_d3d11)
{
	SSTART;

	RUN(surface_d3d11_plan_accepts_windows);
	RUN(surface_d3d11_plan_rejects_non_windows);
	RUN(surface_d3d11_init_null_config);
	RUN(surface_d3d11_init_alloc_failure);
	RUN(surface_d3d11_unbind_null_data);
	RUN(surface_d3d11_free_null_data);
	RUN(surface_d3d11_config_window_null_data);
	RUN(surface_d3d11_config_window_sets_background);
	RUN(surface_d3d11_bind_null_data);
	RUN(surface_d3d11_bind_missing_gfx_native);
	RUN(surface_d3d11_bind_rejects_non_d3d11_gfx);
	RUN(surface_d3d11_bind_rejects_null_device);
	RUN(surface_d3d11_bind_missing_window_native);
	RUN(surface_d3d11_bind_rejects_non_windows_window);
	RUN(surface_d3d11_bind_rejects_null_window_handle);
	RUN(surface_d3d11_bind_missing_library);
	RUN(surface_d3d11_bind_missing_factory_symbol);
	RUN(surface_d3d11_bind_create_factory_failure);
	RUN(surface_d3d11_bind_create_factory_null);
	RUN(surface_d3d11_bind_create_swapchain_failure);
	RUN(surface_d3d11_bind_create_swapchain_null);
	RUN(surface_d3d11_bind_creates_swapchain);
	RUN(surface_d3d11_bind_replaces_swapchain);
	RUN(surface_d3d11_bind_passes_device);
	RUN(surface_d3d11_unbind_releases_swapchain);
	RUN(surface_d3d11_unbind_releases_factory);
	RUN(surface_d3d11_native_sets_api);
	RUN(surface_d3d11_native_returns_ops);
	RUN(surface_d3d11_native_without_bind);
	RUN(surface_d3d11_native_null_data);
	RUN(surface_d3d11_gfx_present_calls_swapchain);
	RUN(surface_d3d11_gfx_present_null_surface);
	RUN(surface_d3d11_gfx_present_failure);

	SEND;
}

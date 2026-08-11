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
typedef void *IDXGIFactory5;
typedef void *IDXGISwapChain;
typedef void *IDXGISwapChain3;

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
	HRESULT (*QueryInterface)(IDXGIFactory *self, REFIID riid, void **object);
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

typedef struct t_factory5_vtbl_s {
	HRESULT (*QueryInterface)(void);
	ULONG (*AddRef)(void);
	ULONG (*Release)(IDXGIFactory5 *self);
	void *methods[25];
	HRESULT (*CheckFeatureSupport)(IDXGIFactory5 *self, UINT feature, void *data, UINT data_size);
} t_factory5_vtbl_t;

typedef struct t_swapchain_vtbl_s {
	HRESULT (*QueryInterface)(IDXGISwapChain *self, REFIID riid, void **object);
	ULONG (*AddRef)(void);
	ULONG (*Release)(IDXGISwapChain *self);
	HRESULT (*SetPrivateData)(void);
	HRESULT (*SetPrivateDataInterface)(void);
	HRESULT (*GetPrivateData)(void);
	HRESULT (*GetParent)(void);
	HRESULT (*GetDevice)(void);
	HRESULT (*Present)(IDXGISwapChain *self, UINT sync_interval, UINT flags);
	HRESULT (*GetBuffer)(void);
	HRESULT (*SetFullscreenState)(void);
	HRESULT (*GetFullscreenState)(void);
	HRESULT (*GetDesc)(void);
	HRESULT (*ResizeBuffers)(IDXGISwapChain *self, UINT buffer_count, UINT width, UINT height, UINT format, UINT flags);
} t_swapchain_vtbl_t;

typedef struct t_swapchain3_vtbl_s {
	HRESULT (*QueryInterface)(void);
	ULONG (*AddRef)(void);
	ULONG (*Release)(IDXGISwapChain3 *self);
	void *methods[33];
	UINT (*GetCurrentBackBufferIndex)(IDXGISwapChain3 *self);
} t_swapchain3_vtbl_t;

typedef struct t_factory_s {
	t_factory_vtbl_t *vtbl;
} t_factory_t;

typedef struct t_factory5_s {
	t_factory5_vtbl_t *vtbl;
} t_factory5_t;

typedef struct t_swapchain_s {
	t_swapchain_vtbl_t *vtbl;
} t_swapchain_t;

typedef struct t_swapchain3_s {
	t_swapchain3_vtbl_t *vtbl;
} t_swapchain3_t;

typedef struct t_dxgi_swapchain_desc_s {
	u8 reserved[40];
	UINT BufferCount;
	HWND OutputWindow;
	int Windowed;
	UINT SwapEffect;
	UINT Flags;
} t_dxgi_swapchain_desc_t;

typedef struct t_surface_d3d11_data_s {
	void *lib;
	IDXGIFactory *factory;
	IDXGIFactory5 *factory5;
	IDXGISwapChain *swapchain;
	IDXGISwapChain3 *swapchain3;
	gfx_surface_t gfx_surface;
	void *CreateDXGIFactory;
	int allow_tearing;
} t_surface_d3d11_data_t;

static int t_create_factory_calls;
static int t_create_swapchain_calls;
static int t_present_calls;
static UINT t_present_sync_interval;
static UINT t_present_flags;
static UINT t_create_swapchain_flags;
static UINT t_resize_flags;
static int t_factory5_supported;
static int t_factory5_query_failure;
static int t_query_swapchain3_calls;
static HRESULT t_query_swapchain3_ret;
static int t_query_swapchain3_null;
static UINT t_back_buffer_index;
static int t_release_factory_calls;
static int t_release_swapchain_calls;
static void *t_create_swapchain_device;
static UINT t_create_swapchain_buffer_count;
static UINT t_create_swapchain_swap_effect;
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
static t_factory5_t t_factory5;
static t_swapchain_t t_swapchain;
static t_swapchain3_t t_swapchain3;
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

static HRESULT t_factory_query_interface(IDXGIFactory *self, REFIID riid, void **object)
{
	(void)self;
	(void)riid;
	*object = t_factory5_query_failure ? NULL : &t_factory5;
	return t_factory5_query_failure ? -1 : S_OK;
}

static ULONG t_factory5_release(IDXGIFactory5 *self)
{
	(void)self;
	t_release_factory_calls++;
	return 0;
}

static HRESULT t_CheckFeatureSupport(IDXGIFactory5 *self, UINT feature, void *data, UINT data_size)
{
	(void)self;
	(void)feature;
	if (data == NULL || data_size != sizeof(int)) {
		return -1;
	}
	*(int *)data = t_factory5_supported;
	return S_OK;
}

static ULONG t_swapchain_release(IDXGISwapChain *self)
{
	(void)self;
	t_release_swapchain_calls++;
	return 0;
}

static ULONG t_swapchain3_release(IDXGISwapChain3 *self)
{
	(void)self;
	t_release_swapchain_calls++;
	return 0;
}

static HRESULT t_swapchain_query_interface(IDXGISwapChain *self, REFIID riid, void **object)
{
	(void)self;
	(void)riid;
	t_query_swapchain3_calls++;
	*object = t_query_swapchain3_ret < 0 || t_query_swapchain3_null ? NULL : &t_swapchain3;
	return t_query_swapchain3_ret;
}

static UINT t_GetCurrentBackBufferIndex(IDXGISwapChain3 *self)
{
	(void)self;
	return t_back_buffer_index;
}

static HRESULT t_CreateSwapChain(IDXGIFactory *self, void *device, void *desc, IDXGISwapChain **swapchain)
{
	(void)self;
	t_create_swapchain_calls++;
	t_create_swapchain_device = device;
	if (desc != NULL) {
		t_create_swapchain_buffer_count = ((const t_dxgi_swapchain_desc_t *)desc)->BufferCount;
		t_create_swapchain_swap_effect	= ((const t_dxgi_swapchain_desc_t *)desc)->SwapEffect;
		t_create_swapchain_flags	= ((const t_dxgi_swapchain_desc_t *)desc)->Flags;
	}
	*swapchain = t_create_swapchain_null ? NULL : (IDXGISwapChain *)&t_swapchain;
	return t_create_swapchain_ret;
}

static HRESULT t_Present(IDXGISwapChain *self, UINT sync_interval, UINT flags)
{
	(void)self;
	t_present_calls++;
	t_present_sync_interval = sync_interval;
	t_present_flags		= flags;
	return t_present_ret;
}

static HRESULT t_ResizeBuffers(IDXGISwapChain *self, UINT buffer_count, UINT width, UINT height, UINT format, UINT flags)
{
	(void)self;
	(void)buffer_count;
	(void)width;
	(void)height;
	(void)format;
	t_resize_flags = flags;
	return S_OK;
}

static HRESULT t_CreateDXGIFactory(REFIID riid, void **factory)
{
	(void)riid;
	t_create_factory_calls++;
	*factory = t_create_factory_null ? NULL : &t_factory;
	return t_create_factory_ret;
}

static t_factory_vtbl_t t_factory_vtbl = {
	.QueryInterface	 = t_factory_query_interface,
	.Release	 = t_factory_release,
	.CreateSwapChain = t_CreateSwapChain,
};

static t_factory5_vtbl_t t_factory5_vtbl = {
	.Release	     = t_factory5_release,
	.CheckFeatureSupport = t_CheckFeatureSupport,
};

static t_swapchain_vtbl_t t_swapchain_vtbl = {
	.QueryInterface = t_swapchain_query_interface,
	.Release	= t_swapchain_release,
	.Present	= t_Present,
	.ResizeBuffers	= t_ResizeBuffers,
};

static t_swapchain3_vtbl_t t_swapchain3_vtbl = {
	.Release		   = t_swapchain3_release,
	.GetCurrentBackBufferIndex = t_GetCurrentBackBufferIndex,
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
	t_create_factory_calls		= 0;
	t_create_swapchain_calls	= 0;
	t_present_calls			= 0;
	t_present_sync_interval		= 0;
	t_present_flags			= 0;
	t_create_swapchain_flags	= 0;
	t_resize_flags			= 0;
	t_factory5_supported		= 1;
	t_factory5_query_failure	= 0;
	t_query_swapchain3_calls	= 0;
	t_query_swapchain3_ret		= S_OK;
	t_query_swapchain3_null		= 0;
	t_back_buffer_index		= 0;
	t_release_factory_calls		= 0;
	t_release_swapchain_calls	= 0;
	t_create_swapchain_device	= NULL;
	t_create_swapchain_buffer_count = 0;
	t_create_swapchain_swap_effect	= 0;
	t_display_native_ret		= 0;
	t_window_native_ret		= 0;
	t_gfx_native_ret		= 0;
	t_display_native_type		= DISPLAY_NATIVE_WINDOWS;
	t_window_native_type		= DISPLAY_NATIVE_WINDOWS;
	t_window_native_window		= (HWND)0x5678;
	t_gfx_native_api		= GFX_API_D3D11;
	t_gfx_native_device		= 0x9876;
	t_create_factory_ret		= S_OK;
	t_create_factory_null		= 0;
	t_create_swapchain_ret		= S_OK;
	t_create_swapchain_null		= 0;
	t_present_ret			= S_OK;
	t_factory.vtbl			= &t_factory_vtbl;
	t_factory5.vtbl			= &t_factory5_vtbl;
	t_swapchain.vtbl		= &t_swapchain_vtbl;
	t_swapchain3.vtbl		= &t_swapchain3_vtbl;
	t_proc				= (proc_t){0};
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
		.surface = {.image_count = 3},
	};

	return surface_init(surface, &config, ALLOC_STD) != surface;
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
	};

	log_set_quiet(0, 1);
	EXPECT_NULL(surface_init(&surface, &config, (alloc_t){.alloc = t_surface_d3d11_alloc_fail}));
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

TEST(surface_d3d11_bind_requires_image_count)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	surface.config.surface.image_count = 0;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_create_factory_calls, 0);
	EXPECT_EQ(t_create_swapchain_calls, 0);

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

TEST(surface_d3d11_bind_query_swapchain3_failure)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_query_swapchain3_ret = -1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_query_swapchain3_calls, 1);
	EXPECT_EQ(t_release_swapchain_calls, 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_query_swapchain3_null)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	t_query_swapchain3_null = 1;

	log_set_quiet(0, 1);
	EXPECT_EQ(surface_bind(&surface, &t_window), 1);
	log_set_quiet(0, 0);
	EXPECT_EQ(t_query_swapchain3_calls, 1);
	EXPECT_EQ(t_release_swapchain_calls, 1);

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
	EXPECT_PTR(t_create_swapchain_device, (void *)(uintptr_t)0x9876);
	EXPECT_EQ(t_create_swapchain_buffer_count, 3);
	EXPECT_EQ(t_create_swapchain_swap_effect, 4);
	EXPECT_EQ(t_create_swapchain_flags, 0x00000800);

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
	EXPECT_EQ(t_release_swapchain_calls, 2);
	EXPECT_EQ(t_release_factory_calls, 2);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_native_returns_surface)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	surface_bind(&surface, &t_window);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->api, GFX_API_D3D11);
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

	native.gfx_surface->ops->present(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT);

	EXPECT_EQ(t_present_calls, 1);
	EXPECT_EQ(t_present_sync_interval, 1);
	EXPECT_EQ(t_present_flags, 0);

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

	EXPECT_EQ(native.gfx_surface->ops->present(NULL, GFX_PRESENT_MODE_DEFAULT), 1);

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

	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT), 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_present_mode_rejects_invalid_args)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	gfx_present_mode_t actual = GFX_PRESENT_MODE_DEFAULT;
	gfx_surface_t empty	  = {0};
	EXPECT_EQ(native.gfx_surface->ops->present_mode(NULL, GFX_PRESENT_MODE_DEFAULT, &actual), 1);
	EXPECT_EQ(native.gfx_surface->ops->present_mode(&empty, GFX_PRESENT_MODE_DEFAULT, &actual), 1);
	EXPECT_EQ(native.gfx_surface->ops->present_mode(native.gfx_surface, GFX_PRESENT_MODE_DEFAULT, NULL), 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_configure_rejects_invalid_args)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	gfx_surface_t empty	    = {0};
	gfx_surface_config_t config = {.width = 1, .height = 1};
	EXPECT_EQ(native.gfx_surface->ops->configure(NULL, &config), 1);
	EXPECT_EQ(native.gfx_surface->ops->configure(&empty, &config), 1);
	EXPECT_EQ(native.gfx_surface->ops->configure(native.gfx_surface, NULL), 1);
	EXPECT_EQ(native.gfx_surface->ops->configure(native.gfx_surface, &(gfx_surface_config_t){.width = 0, .height = 1}), 1);
	EXPECT_EQ(native.gfx_surface->ops->configure(native.gfx_surface, &(gfx_surface_config_t){.width = 1, .height = 0}), 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_configure_requires_swapchain)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	t_surface_d3d11_data_t ctx = {0};
	gfx_surface_t fake	   = {.data = &ctx};
	EXPECT_EQ(native.gfx_surface->ops->configure(&fake, &(gfx_surface_config_t){.width = 1, .height = 1}), 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_acquire_rejects_invalid_args)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	u32 index	    = 0;
	gfx_surface_t empty = {0};
	EXPECT_EQ(native.gfx_surface->ops->acquire(NULL, &index), 1);
	EXPECT_EQ(native.gfx_surface->ops->acquire(&empty, &index), 1);
	EXPECT_EQ(native.gfx_surface->ops->acquire(native.gfx_surface, NULL), 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_acquire_requires_swapchain3)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	t_surface_d3d11_data_t ctx = {0};
	gfx_surface_t fake	   = {.data = &ctx};
	u32 index		   = 0;
	EXPECT_EQ(native.gfx_surface->ops->acquire(&fake, &index), 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_acquire_returns_current_index)
{
	START;

	t_surface_d3d11_reset();
	t_back_buffer_index = 2;
	surface_t surface   = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);
	u32 index = 0;
	EXPECT_EQ(native.gfx_surface->ops->acquire(native.gfx_surface, &index), 0);
	EXPECT_EQ(index, 2);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_immediate_present_allows_tearing)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	gfx_present_mode_t actual = GFX_PRESENT_MODE_DEFAULT;
	EXPECT_EQ(native.gfx_surface->ops->present_mode(native.gfx_surface, GFX_PRESENT_MODE_IMMEDIATE, &actual), 0);
	EXPECT_EQ(actual, GFX_PRESENT_MODE_IMMEDIATE);
	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, actual), 0);
	EXPECT_EQ(t_present_sync_interval, 0);
	EXPECT_EQ(t_present_flags, 0x00000200);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_immediate_falls_back_without_tearing)
{
	START;

	t_surface_d3d11_reset();
	t_factory5_query_failure = 1;
	surface_t surface	 = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	EXPECT_EQ(t_create_swapchain_flags, 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	gfx_present_mode_t actual = GFX_PRESENT_MODE_DEFAULT;
	EXPECT_EQ(native.gfx_surface->ops->present_mode(native.gfx_surface, GFX_PRESENT_MODE_IMMEDIATE, &actual), 0);
	EXPECT_EQ(actual, GFX_PRESENT_MODE_VSYNC);
	EXPECT_EQ(native.gfx_surface->ops->present(native.gfx_surface, actual), 0);
	EXPECT_EQ(t_present_sync_interval, 1);
	EXPECT_EQ(t_present_flags, 0);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_resize_preserves_tearing_flag)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	EXPECT_EQ(t_surface_d3d11_init_surface(&surface), 0);
	EXPECT_EQ(surface_bind(&surface, &t_window), 0);
	surface_native_t native = {0};
	EXPECT_EQ(surface_native(&surface, &native), 0);

	EXPECT_EQ(native.gfx_surface->ops->configure(native.gfx_surface, &(gfx_surface_config_t){.width = 800, .height = 600}), 0);
	EXPECT_EQ(t_resize_flags, 0x00000800);

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
	RUN(surface_d3d11_bind_requires_image_count);
	RUN(surface_d3d11_bind_missing_library);
	RUN(surface_d3d11_bind_missing_factory_symbol);
	RUN(surface_d3d11_bind_create_factory_failure);
	RUN(surface_d3d11_bind_create_factory_null);
	RUN(surface_d3d11_bind_create_swapchain_failure);
	RUN(surface_d3d11_bind_create_swapchain_null);
	RUN(surface_d3d11_bind_query_swapchain3_failure);
	RUN(surface_d3d11_bind_query_swapchain3_null);
	RUN(surface_d3d11_bind_creates_swapchain);
	RUN(surface_d3d11_bind_replaces_swapchain);
	RUN(surface_d3d11_native_returns_surface);
	RUN(surface_d3d11_native_without_bind);
	RUN(surface_d3d11_native_null_data);
	RUN(surface_d3d11_gfx_present_calls_swapchain);
	RUN(surface_d3d11_gfx_immediate_present_allows_tearing);
	RUN(surface_d3d11_gfx_immediate_falls_back_without_tearing);
	RUN(surface_d3d11_gfx_resize_preserves_tearing_flag);
	RUN(surface_d3d11_gfx_present_null_surface);
	RUN(surface_d3d11_gfx_present_failure);
	RUN(surface_d3d11_gfx_present_mode_rejects_invalid_args);
	RUN(surface_d3d11_gfx_configure_rejects_invalid_args);
	RUN(surface_d3d11_gfx_configure_requires_swapchain);
	RUN(surface_d3d11_gfx_acquire_rejects_invalid_args);
	RUN(surface_d3d11_gfx_acquire_requires_swapchain3);
	RUN(surface_d3d11_gfx_acquire_returns_current_index);

	SEND;
}

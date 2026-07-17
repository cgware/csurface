#include "surface_driver.h"

#include "display_driver.h"
#include "gfx_driver.h"
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
static t_factory_t t_factory;
static t_swapchain_t t_swapchain;
static proc_t t_proc;
static display_t t_display;
static gfx_t t_gfx;
static window_t t_window;

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
	*swapchain		 = (IDXGISwapChain *)&t_swapchain;
	return S_OK;
}

static HRESULT t_Present(IDXGISwapChain *self, UINT sync_interval, UINT flags)
{
	(void)self;
	(void)sync_interval;
	(void)flags;
	t_present_calls++;
	return S_OK;
}

static HRESULT t_CreateDXGIFactory(REFIID riid, void **factory)
{
	(void)riid;
	t_create_factory_calls++;
	*factory = &t_factory;
	return S_OK;
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
		.type	 = DISPLAY_NATIVE_WINDOWS,
		.display = (void *)0x1234,
	};
	return 0;
}

static int t_window_native(window_t *window, window_native_t *native)
{
	(void)window;
	*native = (window_native_t){
		.type	= DISPLAY_NATIVE_WINDOWS,
		.window = (HWND)0x5678,
	};
	return 0;
}

static int t_gfx_native(gfx_t *gfx, gfx_native_t *native)
{
	(void)gfx;
	*native = (gfx_native_t){
		.api	= GFX_API_D3D11,
		.device = 0x9876,
	};
	return 0;
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
	t_create_factory_calls	 = 0;
	t_create_swapchain_calls = 0;
	t_present_calls		 = 0;
	t_release_factory_calls	 = 0;
	t_release_swapchain_calls = 0;
	t_create_swapchain_device = NULL;
	t_factory.vtbl		 = &t_factory_vtbl;
	t_swapchain.vtbl	 = &t_swapchain_vtbl;
	t_proc			 = (proc_t){0};
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

TEST(surface_d3d11_plan_accepts_windows)
{
	START;

	t_surface_d3d11_reset();
	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &t_display, .gfx_api = GFX_API_D3D11}), 0);

	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_creates_swapchain)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	surface_init(&surface, &(surface_config_t){.display = &t_display, .gfx = &t_gfx, .alloc = ALLOC_STD});

	surface_bind(&surface, &t_window);

	EXPECT_EQ(t_create_swapchain_calls, 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_bind_passes_device)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	surface_init(&surface, &(surface_config_t){.display = &t_display, .gfx = &t_gfx, .alloc = ALLOC_STD});

	surface_bind(&surface, &t_window);

	EXPECT_EQ(t_create_swapchain_device, (void *)(uintptr_t)0x9876);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_native_sets_api)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	surface_init(&surface, &(surface_config_t){.display = &t_display, .gfx = &t_gfx, .alloc = ALLOC_STD});
	surface_bind(&surface, &t_window);
	surface_native_t native = {0};
	surface_native(&surface, &native);

	EXPECT_EQ(native.gfx_surface->api, GFX_API_D3D11);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

TEST(surface_d3d11_gfx_present_calls_swapchain)
{
	START;

	t_surface_d3d11_reset();
	surface_t surface = {0};
	surface_init(&surface, &(surface_config_t){.display = &t_display, .gfx = &t_gfx, .alloc = ALLOC_STD});
	surface_bind(&surface, &t_window);
	surface_native_t native = {0};
	surface_native(&surface, &native);

	native.gfx_surface->ops->present(native.gfx_surface);

	EXPECT_EQ(t_present_calls, 1);

	surface_free(&surface);
	t_surface_d3d11_cleanup();
	END;
}

STEST(surface_d3d11)
{
	SSTART;

	RUN(surface_d3d11_plan_accepts_windows);
	RUN(surface_d3d11_bind_creates_swapchain);
	RUN(surface_d3d11_bind_passes_device);
	RUN(surface_d3d11_native_sets_api);
	RUN(surface_d3d11_gfx_present_calls_swapchain);

	SEND;
}

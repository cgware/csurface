#include "surface_driver.h"

#include "log.h"
#include "mem.h"

typedef long HRESULT;
typedef unsigned int UINT;
typedef unsigned long ULONG;
typedef unsigned int DXGI_FORMAT;
typedef unsigned int DXGI_USAGE;
typedef void *HWND;
typedef void *IDXGIFactory;
typedef void *IDXGISwapChain;

enum {
	S_OK				     = 0,
	DXGI_FORMAT_R8G8B8A8_UNORM	     = 28,
	DXGI_USAGE_RENDER_TARGET_OUTPUT	     = 0x00000020,
	DXGI_SWAP_EFFECT_DISCARD	     = 0,
	DXGI_SWAP_CHAIN_WINDOWED	     = 1,
	SURFACE_D3D11_SWAPCHAIN_BUFFER_COUNT = 2,
};

typedef struct GUID_s {
	u32 Data1;
	u16 Data2;
	u16 Data3;
	u8 Data4[8];
} GUID;

typedef const GUID *REFIID;

static const GUID IID_IDXGIFactory = {0x7b7166ecu, 0x21c7u, 0x44aeu, {0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69}};

typedef struct DXGI_RATIONAL_s {
	UINT Numerator;
	UINT Denominator;
} DXGI_RATIONAL;

typedef struct DXGI_MODE_DESC_s {
	UINT Width;
	UINT Height;
	DXGI_RATIONAL RefreshRate;
	DXGI_FORMAT Format;
	UINT ScanlineOrdering;
	UINT Scaling;
} DXGI_MODE_DESC;

typedef struct DXGI_SAMPLE_DESC_s {
	UINT Count;
	UINT Quality;
} DXGI_SAMPLE_DESC;

typedef struct DXGI_SWAP_CHAIN_DESC_s {
	DXGI_MODE_DESC BufferDesc;
	DXGI_SAMPLE_DESC SampleDesc;
	DXGI_USAGE BufferUsage;
	UINT BufferCount;
	HWND OutputWindow;
	int Windowed;
	UINT SwapEffect;
	UINT Flags;
} DXGI_SWAP_CHAIN_DESC;

typedef struct IDXGIFactoryVTable_s {
	HRESULT (*QueryInterface)(IDXGIFactory *self, REFIID riid, void **object);
	ULONG (*AddRef)(IDXGIFactory *self);
	ULONG (*Release)(IDXGIFactory *self);
	HRESULT (*SetPrivateData)(void);
	HRESULT (*SetPrivateDataInterface)(void);
	HRESULT (*GetPrivateData)(void);
	HRESULT (*GetParent)(void);
	HRESULT (*EnumAdapters)(void);
	HRESULT (*MakeWindowAssociation)(void);
	HRESULT (*GetWindowAssociation)(void);
	HRESULT (*CreateSwapChain)(IDXGIFactory *self, void *device, DXGI_SWAP_CHAIN_DESC *desc, IDXGISwapChain **swapchain);
} IDXGIFactoryVTable;

typedef struct IDXGISwapChainVTable_s {
	HRESULT (*QueryInterface)(IDXGISwapChain *self, REFIID riid, void **object);
	ULONG (*AddRef)(IDXGISwapChain *self);
	ULONG (*Release)(IDXGISwapChain *self);
	HRESULT (*SetPrivateData)(void);
	HRESULT (*SetPrivateDataInterface)(void);
	HRESULT (*GetPrivateData)(void);
	HRESULT (*GetParent)(void);
	HRESULT (*GetDevice)(void);
	HRESULT (*Present)(IDXGISwapChain *self, UINT sync_interval, UINT flags);
} IDXGISwapChainVTable;

typedef HRESULT (*PFN_CreateDXGIFactory)(REFIID riid, void **factory);

typedef struct surface_d3d11_s {
	void *lib;
	IDXGIFactory *factory;
	IDXGISwapChain *swapchain;
	gfx_surface_t gfx_surface;
	PFN_CreateDXGIFactory CreateDXGIFactory;
} surface_d3d11_t;

static int hresult_ok(HRESULT hr)
{
	return hr >= 0;
}

static ULONG d3d11_release(void *object)
{
	void ***iface		 = object;
	ULONG (**vtable)(void *) = (ULONG(**)(void *)) * iface;
	return vtable[2](object);
}

static int surface_d3d11_compatible(const surface_info_t *info)
{
	return info != NULL && info->gfx_api == GFX_API_D3D11 && info->native_type == DISPLAY_NATIVE_WINDOWS;
}

static int surface_d3d11_init(surface_t *srf, const surface_config_t *config)
{
	if (srf == NULL || config == NULL || config->alloc.alloc == NULL) {
		return 1;
	}

	alloc_t alloc	     = config->alloc;
	surface_d3d11_t *ctx = alloc_alloc(&alloc, sizeof(*ctx));
	if (ctx == NULL) {
		log_error("csurface", "surface_d3d11", NULL, "failed to allocate surface data");
		return 1;
	}
	mem_set(ctx, 0, sizeof(*ctx));
	srf->data = ctx;
	return 0;
}

static int surface_d3d11_unbind(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_d3d11_t *ctx = srf->data;
	if (ctx->swapchain != NULL) {
		d3d11_release(ctx->swapchain);
	}
	if (ctx->factory != NULL) {
		d3d11_release(ctx->factory);
	}
	if (ctx->lib != NULL) {
		proc_dlclose(srf->config.display->proc, ctx->lib);
	}
	ctx->lib	 = NULL;
	ctx->factory	 = NULL;
	ctx->swapchain	 = NULL;
	ctx->gfx_surface = (gfx_surface_t){0};
	return 0;
}

static int surface_d3d11_free(surface_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_d3d11_t *ctx = srf->data;
	surface_d3d11_unbind(srf);
	alloc_free(&srf->config.alloc, ctx, sizeof(*ctx));
	srf->data = NULL;
	return 0;
}

static int surface_d3d11_config_window(surface_t *srf, window_config_t *config)
{
	if (srf == NULL || srf->data == NULL || config == NULL) {
		return 1;
	}

	config->background = WINDOW_BACKGROUND_NONE;
	return 0;
}

static int surface_d3d11_load(surface_t *srf, surface_d3d11_t *ctx)
{
	if (proc_dlopen(srf->config.display->proc, STRV("dxgi.dll"), &ctx->lib)) {
		log_error("csurface", "surface_d3d11", NULL, "failed to load DXGI library");
		return 1;
	}
	if (proc_dlsym(srf->config.display->proc, ctx->lib, STRV("CreateDXGIFactory"), (void **)&ctx->CreateDXGIFactory)) {
		log_error("csurface", "surface_d3d11", NULL, "failed to load DXGI symbol: CreateDXGIFactory");
		return 1;
	}

	return 0;
}

static const gfx_surface_ops_t surface_d3d11_gfx_ops;

static int surface_d3d11_bind(surface_t *srf, window_t *window)
{
	if (srf == NULL || srf->data == NULL || window == NULL) {
		return 1;
	}

	gfx_native_t native_gfx = {0};
	if (gfx_native(srf->config.gfx, &native_gfx) || native_gfx.api != GFX_API_D3D11 || native_gfx.device == 0) {
		log_error("csurface", "surface_d3d11", NULL, "D3D11 native gfx device is unavailable");
		return 1;
	}

	window_native_t native_window = {0};
	if (window_native(window, &native_window) || native_window.type != DISPLAY_NATIVE_WINDOWS || native_window.window == NULL) {
		log_error("csurface", "surface_d3d11", NULL, "Windows native window is unavailable");
		return 1;
	}

	surface_d3d11_t *ctx = srf->data;
	if (ctx->swapchain != NULL) {
		surface_d3d11_unbind(srf);
	}
	if (surface_d3d11_load(srf, ctx)) {
		surface_d3d11_unbind(srf);
		return 1;
	}
	if (!hresult_ok(ctx->CreateDXGIFactory(&IID_IDXGIFactory, (void **)&ctx->factory)) || ctx->factory == NULL) {
		log_error("csurface", "surface_d3d11", NULL, "failed to create DXGI factory");
		surface_d3d11_unbind(srf);
		return 1;
	}

	DXGI_SWAP_CHAIN_DESC desc = {
		.BufferDesc =
			{
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			},
		.SampleDesc =
			{
				.Count = 1,
			},
		.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount  = SURFACE_D3D11_SWAPCHAIN_BUFFER_COUNT,
		.OutputWindow = native_window.window,
		.Windowed     = DXGI_SWAP_CHAIN_WINDOWED,
		.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD,
	};

	IDXGIFactoryVTable *factory = *(IDXGIFactoryVTable **)ctx->factory;
	if (!hresult_ok(factory->CreateSwapChain(ctx->factory, (void *)(uintptr_t)native_gfx.device, &desc, &ctx->swapchain)) ||
	    ctx->swapchain == NULL) {
		log_error("csurface", "surface_d3d11", NULL, "failed to create DXGI swapchain");
		surface_d3d11_unbind(srf);
		return 1;
	}

	ctx->gfx_surface = (gfx_surface_t){
		.api	= GFX_API_D3D11,
		.handle = (u64)(uintptr_t)ctx->swapchain,
		.data	= ctx,
		.ops	= &surface_d3d11_gfx_ops,
	};
	return 0;
}

static int surface_d3d11_gfx_present(gfx_surface_t *surface)
{
	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_d3d11_t *ctx	   = surface->data;
	IDXGISwapChainVTable *swap = *(IDXGISwapChainVTable **)ctx->swapchain;
	return hresult_ok(swap->Present(ctx->swapchain, 1, 0)) ? 0 : 1;
}

static const gfx_surface_ops_t surface_d3d11_gfx_ops = {
	.present = surface_d3d11_gfx_present,
};

static int surface_d3d11_native(surface_t *srf, surface_native_t *native)
{
	if (srf == NULL || srf->data == NULL || native == NULL) {
		return 1;
	}

	surface_d3d11_t *ctx = srf->data;
	if (ctx->swapchain == NULL) {
		return 1;
	}

	*native = (surface_native_t){
		.gfx_api     = GFX_API_D3D11,
		.native_type = DISPLAY_NATIVE_WINDOWS,
		.handle	     = (u64)(uintptr_t)ctx->swapchain,
		.gfx_surface = &ctx->gfx_surface,
	};
	return 0;
}

static surface_driver_t surface_d3d11 = {
	.name	       = "d3d11",
	.compatible    = surface_d3d11_compatible,
	.init	       = surface_d3d11_init,
	.free	       = surface_d3d11_free,
	.config_window = surface_d3d11_config_window,
	.bind	       = surface_d3d11_bind,
	.unbind	       = surface_d3d11_unbind,
	.native	       = surface_d3d11_native,
};

SURFACE_DRIVER(surface_d3d11, &surface_d3d11);

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
typedef void *IDXGIFactory5;
typedef void *IDXGISwapChain;
typedef void *IDXGISwapChain3;

enum {
	S_OK				   = 0,
	DXGI_FORMAT_R8G8B8A8_UNORM	   = 28,
	DXGI_USAGE_RENDER_TARGET_OUTPUT	   = 0x00000020,
	DXGI_SWAP_EFFECT_DISCARD	   = 0,
	DXGI_SWAP_EFFECT_FLIP_DISCARD	   = 4,
	DXGI_SWAP_CHAIN_WINDOWED	   = 1,
	DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING = 0x00000800,
	DXGI_PRESENT_ALLOW_TEARING	   = 0x00000200,
	DXGI_FEATURE_PRESENT_ALLOW_TEARING = 0,
};

typedef struct GUID_s {
	u32 Data1;
	u16 Data2;
	u16 Data3;
	u8 Data4[8];
} GUID;

typedef const GUID *REFIID;

static const GUID IID_IDXGIFactory    = {0x7b7166ecu, 0x21c7u, 0x44aeu, {0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69}};
static const GUID IID_IDXGIFactory5   = {0x7632e1f5u, 0xee65u, 0x4dcau, {0x87, 0xfd, 0x84, 0xcd, 0x75, 0xf8, 0x83, 0x8d}};
static const GUID IID_IDXGISwapChain3 = {0x94d99bdbu, 0xf1f8u, 0x4ab0u, {0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1}};

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
	HRESULT (*GetBuffer)(void);
	HRESULT (*SetFullscreenState)(void);
	HRESULT (*GetFullscreenState)(void);
	HRESULT (*GetDesc)(void);
	HRESULT (*ResizeBuffers)(IDXGISwapChain *self, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT format, UINT flags);
} IDXGISwapChainVTable;

typedef struct IDXGIFactory5VTable_s {
	HRESULT (*QueryInterface)(IDXGIFactory5 *self, REFIID riid, void **object);
	ULONG (*AddRef)(IDXGIFactory5 *self);
	ULONG (*Release)(IDXGIFactory5 *self);
	void *methods[25];
	HRESULT (*CheckFeatureSupport)(IDXGIFactory5 *self, UINT feature, void *data, UINT data_size);
} IDXGIFactory5VTable;

typedef struct IDXGISwapChain3VTable_s {
	HRESULT (*QueryInterface)(IDXGISwapChain3 *self, REFIID riid, void **object);
	ULONG (*AddRef)(IDXGISwapChain3 *self);
	ULONG (*Release)(IDXGISwapChain3 *self);
	void *methods[33];
	UINT (*GetCurrentBackBufferIndex)(IDXGISwapChain3 *self);
} IDXGISwapChain3VTable;

typedef HRESULT (*PFN_CreateDXGIFactory)(REFIID riid, void **factory);

typedef struct surface_d3d11_s {
	void *lib;
	IDXGIFactory *factory;
	IDXGIFactory5 *factory5;
	IDXGISwapChain *swapchain;
	IDXGISwapChain3 *swapchain3;
	gfx_surface_t gfx_surface;
	PFN_CreateDXGIFactory CreateDXGIFactory;
	int allow_tearing;
	int flip_model;
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

static int surface_d3d11_init(surface_backend_t *srf, const surface_backend_config_t *config)
{
	if (srf == NULL || config == NULL) {
		return 1;
	}

	surface_d3d11_t *ctx = alloc_alloc(&srf->alloc, sizeof(*ctx));
	if (ctx == NULL) {
		log_error("csurface", "surface_d3d11", NULL, "failed to allocate surface data");
		return 1;
	}
	mem_set(ctx, 0, sizeof(*ctx));
	srf->data = ctx;
	return 0;
}

static int surface_d3d11_unbind(surface_backend_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_d3d11_t *ctx = srf->data;
	if (ctx->swapchain3 != NULL) {
		d3d11_release(ctx->swapchain3);
	}
	if (ctx->swapchain != NULL) {
		d3d11_release(ctx->swapchain);
	}
	if (ctx->factory5 != NULL) {
		d3d11_release(ctx->factory5);
	}
	if (ctx->factory != NULL) {
		d3d11_release(ctx->factory);
	}
	if (ctx->lib != NULL) {
		proc_dlclose(srf->config.display->proc, ctx->lib);
	}
	ctx->lib	   = NULL;
	ctx->factory	   = NULL;
	ctx->factory5	   = NULL;
	ctx->swapchain	   = NULL;
	ctx->swapchain3	   = NULL;
	ctx->allow_tearing = 0;
	ctx->gfx_surface   = (gfx_surface_t){0};
	return 0;
}

static int surface_d3d11_free(surface_backend_t *srf)
{
	if (srf == NULL || srf->data == NULL) {
		return 1;
	}

	surface_d3d11_t *ctx = srf->data;
	surface_d3d11_unbind(srf);
	alloc_free(&srf->alloc, ctx, sizeof(*ctx));
	srf->data = NULL;
	return 0;
}

static int surface_d3d11_config_window(surface_backend_t *srf, window_config_t *config)
{
	if (srf == NULL || srf->data == NULL || config == NULL) {
		return 1;
	}

	config->background = WINDOW_BACKGROUND_NONE;
	return 0;
}

static int surface_d3d11_load(surface_backend_t *srf, surface_d3d11_t *ctx)
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

static int surface_d3d11_bind(surface_backend_t *srf, window_t *window)
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
	if (srf->config.surface.image_count < 2) {
		log_error("csurface", "surface_d3d11", NULL, "D3D11 flip-model surface requires at least two swapchain images");
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
	IDXGIFactoryVTable *factory = *(IDXGIFactoryVTable **)ctx->factory;
	if (hresult_ok(factory->QueryInterface(ctx->factory, &IID_IDXGIFactory5, (void **)&ctx->factory5)) && ctx->factory5 != NULL) {
		IDXGIFactory5VTable *factory5 = *(IDXGIFactory5VTable **)ctx->factory5;
		int supported		      = 0;
		if (hresult_ok(factory5->CheckFeatureSupport(
			    ctx->factory5, DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported)))) {
			ctx->allow_tearing = supported != 0;
		}
	}

	ctx->flip_model		  = !srf->config.api_switching;
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
		.BufferCount  = srf->config.surface.image_count,
		.OutputWindow = native_window.window,
		.Windowed     = DXGI_SWAP_CHAIN_WINDOWED,
		.SwapEffect   = ctx->flip_model ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_DISCARD,
		.Flags	      = ctx->flip_model && ctx->allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0,
	};

	if (!hresult_ok(factory->CreateSwapChain(ctx->factory, (void *)(uintptr_t)native_gfx.device, &desc, &ctx->swapchain)) ||
	    ctx->swapchain == NULL) {
		log_error("csurface", "surface_d3d11", NULL, "failed to create DXGI swapchain");
		surface_d3d11_unbind(srf);
		return 1;
	}
	if (ctx->flip_model) {
		IDXGISwapChainVTable *swap = *(IDXGISwapChainVTable **)ctx->swapchain;
		if (!hresult_ok(swap->QueryInterface(ctx->swapchain, &IID_IDXGISwapChain3, (void **)&ctx->swapchain3)) ||
		    ctx->swapchain3 == NULL) {
			log_error("csurface", "surface_d3d11", NULL, "failed to get DXGI 1.4 swapchain interface");
			surface_d3d11_unbind(srf);
			return 1;
		}
	}

	ctx->gfx_surface = (gfx_surface_t){
		.api	= GFX_API_D3D11,
		.handle = (u64)(uintptr_t)ctx->swapchain,
		.data	= ctx,
		.ops	= &surface_d3d11_gfx_ops,
	};
	return 0;
}

static int surface_d3d11_gfx_present_mode(gfx_surface_t *surface, gfx_present_mode_t requested, gfx_present_mode_t *actual)
{
	if (surface == NULL || surface->data == NULL || actual == NULL) {
		return 1;
	}
	surface_d3d11_t *ctx = surface->data;
	*actual = requested == GFX_PRESENT_MODE_IMMEDIATE && (!ctx->flip_model || ctx->allow_tearing) ? GFX_PRESENT_MODE_IMMEDIATE
												      : GFX_PRESENT_MODE_VSYNC;
	return 0;
}

static int surface_d3d11_gfx_configure(gfx_surface_t *surface, const gfx_surface_config_t *config)
{
	if (surface == NULL || surface->data == NULL || config == NULL || config->width == 0 || config->height == 0) {
		return 1;
	}
	surface_d3d11_t *ctx = surface->data;
	if (ctx->swapchain == NULL) {
		return 1;
	}
	IDXGISwapChainVTable *swap = *(IDXGISwapChainVTable **)ctx->swapchain;
	UINT flags		   = ctx->flip_model && ctx->allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	return hresult_ok(swap->ResizeBuffers(ctx->swapchain, 0, config->width, config->height, 0, flags)) ? 0 : 1;
}

static int surface_d3d11_gfx_present(gfx_surface_t *surface, gfx_present_mode_t present_mode)
{
	if (surface == NULL || surface->data == NULL) {
		return 1;
	}

	surface_d3d11_t *ctx	   = surface->data;
	IDXGISwapChainVTable *swap = *(IDXGISwapChainVTable **)ctx->swapchain;
	UINT immediate		   = present_mode == GFX_PRESENT_MODE_IMMEDIATE;
	UINT flags		   = immediate && ctx->flip_model && ctx->allow_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
	return hresult_ok(swap->Present(ctx->swapchain, immediate ? 0 : 1, flags)) ? 0 : 1;
}

static int surface_d3d11_gfx_acquire(gfx_surface_t *surface, u32 *image_index)
{
	if (surface == NULL || surface->data == NULL || image_index == NULL) {
		return 1;
	}

	surface_d3d11_t *ctx = surface->data;
	if (!ctx->flip_model) {
		*image_index = 0;
		return 0;
	}
	if (ctx->swapchain3 == NULL) {
		return 1;
	}
	IDXGISwapChain3VTable *swap = *(IDXGISwapChain3VTable **)ctx->swapchain3;
	*image_index		    = swap->GetCurrentBackBufferIndex(ctx->swapchain3);
	return 0;
}

static const gfx_surface_ops_t surface_d3d11_gfx_ops = {
	.present_mode = surface_d3d11_gfx_present_mode,
	.configure    = surface_d3d11_gfx_configure,
	.acquire      = surface_d3d11_gfx_acquire,
	.present      = surface_d3d11_gfx_present,
};

static int surface_d3d11_native(surface_backend_t *srf, surface_native_t *native)
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

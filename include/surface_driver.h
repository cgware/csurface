#ifndef SURFACE_DRIVER_H
#define SURFACE_DRIVER_H

#include "driver.h"
#include "surface.h"

typedef enum surface_gfx_init_order_e {
	SURFACE_GFX_INIT_BEFORE_BIND,
	SURFACE_GFX_INIT_AFTER_BIND,
} surface_gfx_init_order_t;

typedef struct surface_backend_config_s {
	display_t *display;
	gfx_t *gfx;
	gfx_api_t gfx_api;
	gfx_surface_config_t surface;
	int api_switching;
} surface_backend_config_t;

struct surface_backend_s {
	const struct surface_driver_s *drv;
	alloc_t alloc;
	surface_backend_config_t config;
	void *data;
};

typedef struct surface_driver_s {
	const char *name;
	surface_gfx_init_order_t gfx_init_order;
	int (*compatible)(const surface_info_t *info);
	int (*plan)(const surface_info_t *info, surface_plan_t *plan);
	int (*init)(surface_backend_t *srf, const surface_backend_config_t *config);
	int (*free)(surface_backend_t *srf);
	int (*config_window)(surface_backend_t *srf, window_config_t *config);
	int (*bind)(surface_backend_t *srf, window_t *window);
	int (*unbind)(surface_backend_t *srf);
	int (*native)(surface_backend_t *srf, surface_native_t *native);
} surface_driver_t;

surface_backend_t *surface_backend_init(surface_backend_t *srf, const surface_backend_config_t *config, alloc_t alloc);
void surface_backend_free(surface_backend_t *srf);
int surface_backend_config_window(surface_backend_t *srf, window_config_t *config);
int surface_backend_bind(surface_backend_t *srf, window_t *window);
int surface_backend_unbind(surface_backend_t *srf);
int surface_backend_native(surface_backend_t *srf, surface_native_t *native);

#define SURFACE_DRIVER_TYPE 0x535246

#define SURFACE_DRIVER(_name, _data) DRIVER(_name, SURFACE_DRIVER_TYPE, _data)

#endif

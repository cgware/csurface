#ifndef SURFACE_H
#define SURFACE_H

#include "display.h"
#include "gfx.h"
#include "window.h"

typedef struct surface_info_s {
	gfx_api_t gfx_api;
	display_native_type_t native_type;
} surface_info_t;

typedef struct surface_plan_config_s {
	display_t *display;
	gfx_api_t gfx_api;
} surface_plan_config_t;

typedef struct surface_plan_s {
	gfx_plan_t gfx;
} surface_plan_t;

typedef struct surface_native_s {
	gfx_api_t gfx_api;
	display_native_type_t native_type;
	void *display;
	void *visual;
	u64 handle;
	gfx_surface_t *gfx_surface;
} surface_native_t;

typedef struct surface_config_s {
	display_t *display;
	gfx_t *gfx;
	alloc_t alloc;
} surface_config_t;

typedef struct surface_s {
	const struct surface_driver_s *drv;
	surface_config_t config;
	void *data;
} surface_t;

int surface_plan(surface_plan_t *plan, const surface_plan_config_t *config);
surface_t *surface_init(surface_t *srf, const surface_config_t *config);
void surface_free(surface_t *srf);
int surface_config_window(surface_t *srf, window_config_t *config);
int surface_bind(surface_t *srf, window_t *window);
int surface_unbind(surface_t *srf);
int surface_native(surface_t *srf, surface_native_t *native);

#endif

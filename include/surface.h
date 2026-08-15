#ifndef SURFACE_H
#define SURFACE_H

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
	const struct gfx_driver_s *driver;
	gfx_surface_config_t surface;
	int api_switching;
} surface_config_t;

typedef struct surface_backend_s surface_backend_t;

typedef struct surface_s {
	surface_backend_t *backend;
	gfx_t gfx;
	surface_config_t config;
	proc_t *proc;
	alloc_t alloc;
} surface_t;

int surface_plan(surface_plan_t *plan, const surface_plan_config_t *config);

int surface_supported(const surface_config_t *config);
int surface_init(surface_t *surface, const surface_config_t *config, proc_t *proc, alloc_t alloc);
int surface_config_window(surface_t *surface, window_config_t *config);
int surface_bind(surface_t *surface, window_t *window);
int surface_unbind(surface_t *surface);
int surface_native(surface_t *surface, surface_native_t *native);
gfx_t *surface_gfx(surface_t *surface);
void surface_free(surface_t *srf);

#endif

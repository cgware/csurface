#include "surface.h"

#include "gfx_driver.h"
#include "surface_driver.h"

static int surface_backend_config_valid(const surface_backend_config_t *config)
{
	return config != NULL && config->display != NULL && (config->gfx != NULL || config->gfx_api != GFX_API_NONE);
}

static int surface_plan_config_valid(const surface_plan_config_t *config)
{
	return config != NULL && config->display != NULL;
}

static int surface_config_valid(const surface_config_t *config)
{
	return config != NULL && config->display != NULL && config->driver != NULL && config->driver->api != GFX_API_NONE;
}

static void surface_info_display(display_t *display, gfx_api_t gfx_api, surface_info_t *info)
{
	info->gfx_api = gfx_api;

	display_native_t native = {0};
	if (display_native(display, &native)) {
		info->native_type = DISPLAY_NATIVE_NONE;
		return;
	}

	info->native_type = native.type;
}

static int surface_backend_info(const surface_backend_config_t *config, surface_info_t *info)
{
	gfx_native_t native_gfx = {0};
	if (!surface_backend_config_valid(config) || info == NULL) {
		return 1; // LCOV_EXCL_LINE
	}

	if (config->gfx != NULL) {
		if (gfx_native(config->gfx, &native_gfx)) {
			return 1;
		}
	} else {
		native_gfx.api = config->gfx_api;
	}
	surface_info_display(config->display, native_gfx.api, info);
	return 0;
}

static surface_driver_t *surface_driver_compatible(const surface_info_t *info)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != SURFACE_DRIVER_TYPE) {
			continue;
		}

		surface_driver_t *drv = i->data;
		if (drv == NULL || drv->compatible == NULL || !drv->compatible(info)) {
			continue;
		}
		return drv;
	}

	return NULL;
}

int surface_plan(surface_plan_t *plan, const surface_plan_config_t *config)
{
	if (plan == NULL || !surface_plan_config_valid(config)) {
		return 1;
	}

	surface_info_t info = {0};
	surface_info_display(config->display, config->gfx_api, &info);

	surface_driver_t *drv = surface_driver_compatible(&info);
	if (drv == NULL) {
		return 1;
	}
	if (drv->plan == NULL) {
		return 0;
	}

	return drv->plan(&info, plan);
}

static surface_driver_t *surface_driver_for_config(const surface_config_t *config, surface_info_t *info)
{
	if (!surface_config_valid(config) || info == NULL) {
		return NULL; // LCOV_EXCL_LINE
	}

	surface_info_display(config->display, config->driver->api, info);
	return surface_driver_compatible(info);
}

int surface_supported(const surface_config_t *config)
{
	if (!surface_config_valid(config)) {
		return 0;
	}

	surface_info_t info   = {0};
	surface_driver_t *drv = surface_driver_for_config(config, &info);
	if (drv == NULL) {
		return 0;
	}
	if (drv->plan == NULL) {
		return 1;
	}

	surface_plan_t plan = {0};
	return drv->plan(&info, &plan) == 0;
}

static surface_backend_t *surface_backend_alloc(alloc_t alloc)
{
	surface_backend_t *backend = alloc_alloc(&alloc, sizeof(*backend));
	if (backend == NULL) {
		return NULL;
	}
	*backend = (surface_backend_t){0};
	return backend;
}

static void surface_backend_dealloc(surface_backend_t *backend)
{
	if (backend == NULL) {
		return; // LCOV_EXCL_LINE
	}

	alloc_t alloc = backend->alloc;
	alloc_free(&alloc, backend, sizeof(*backend));
}

static surface_backend_t *surface_backend_init_driver(surface_backend_t *backend, const surface_driver_t *drv,
						      const surface_backend_config_t *config, alloc_t alloc)
{
	if (backend == NULL || drv == NULL || drv->init == NULL || !surface_backend_config_valid(config)) {
		return NULL;
	}

	*backend = (surface_backend_t){
		.drv	= drv,
		.alloc	= alloc,
		.config = *config,
	};
	if (backend->drv->init(backend, config)) {
		if (backend->drv->free != NULL) {
			backend->drv->free(backend);
		}
		backend->drv	= NULL;
		backend->config = (surface_backend_config_t){0};
		backend->data	= NULL;
		return NULL;
	}

	return backend;
}

surface_backend_t *surface_backend_init(surface_backend_t *backend, const surface_backend_config_t *config, alloc_t alloc)
{
	if (!surface_backend_config_valid(config)) {
		return NULL;
	}

	surface_info_t info = {0};
	if (surface_backend_info(config, &info)) {
		return NULL;
	}

	surface_driver_t *drv = surface_driver_compatible(&info);
	if (drv == NULL) {
		return NULL;
	}
	return surface_backend_init_driver(backend, drv, config, alloc);
}

void surface_backend_free(surface_backend_t *backend)
{
	if (backend == NULL) {
		return;
	}

	if (backend->drv != NULL && backend->drv->free != NULL) {
		backend->drv->free(backend);
	}
	*backend = (surface_backend_t){0};
}

static void surface_backend_destroy(surface_backend_t *backend)
{
	if (backend == NULL) {
		return;
	}

	alloc_t alloc = backend->alloc;
	surface_backend_free(backend);
	alloc_free(&alloc, backend, sizeof(*backend));
}

static surface_backend_t *surface_backend_create_driver(const surface_driver_t *drv, const surface_backend_config_t *config, alloc_t alloc)
{
	surface_backend_t *backend = surface_backend_alloc(alloc);
	if (backend == NULL) {
		return NULL;
	}
	if (surface_backend_init_driver(backend, drv, config, alloc) == NULL) {
		surface_backend_dealloc(backend);
		return NULL;
	}
	return backend;
}

int surface_backend_config_window(surface_backend_t *backend, window_config_t *config)
{
	if (backend == NULL || backend->drv == NULL || backend->drv->config_window == NULL || config == NULL) {
		return 1;
	}

	return backend->drv->config_window(backend, config);
}

int surface_backend_bind(surface_backend_t *backend, window_t *window)
{
	if (backend == NULL || backend->drv == NULL || backend->drv->bind == NULL || window == NULL) {
		return 1;
	}

	return backend->drv->bind(backend, window);
}

int surface_backend_unbind(surface_backend_t *backend)
{
	if (backend == NULL || backend->drv == NULL || backend->drv->unbind == NULL) {
		return 1;
	}

	return backend->drv->unbind(backend);
}

int surface_backend_native(surface_backend_t *backend, surface_native_t *native)
{
	if (backend == NULL || backend->drv == NULL || backend->drv->native == NULL || native == NULL) {
		return 1;
	}

	return backend->drv->native(backend, native);
}

int surface_init(surface_t *surface, const surface_config_t *config, proc_t *proc, alloc_t alloc)
{
	if (surface == NULL || !surface_config_valid(config)) {
		return 1;
	}

	surface_info_t info   = {0};
	surface_driver_t *drv = surface_driver_for_config(config, &info);
	if (drv == NULL) {
		return 1;
	}

	surface_plan_t plan = {0};
	if (drv->plan != NULL && drv->plan(&info, &plan)) {
		return 1;
	}

	*surface = (surface_t){
		.config = *config,
		.proc	= proc,
		.alloc	= alloc,
	};

	if (drv->gfx_init_order == SURFACE_GFX_INIT_AFTER_BIND) {
		surface->backend = surface_backend_create_driver(drv,
								 &(surface_backend_config_t){
									 .display	= config->display,
									 .gfx_api	= config->driver->api,
									 .surface	= config->surface,
									 .api_switching = config->api_switching,
								 },
								 alloc);
		if (surface->backend == NULL) {
			*surface = (surface_t){0};
			return 1;
		}
		return 0;
	}

	if (gfx_init(&surface->gfx,
		     config->driver,
		     &(gfx_config_t){
			     .plan = &plan.gfx,
		     },
		     proc,
		     alloc) == NULL) {
		return 1;
	}
	surface->backend = surface_backend_create_driver(drv,
							 &(surface_backend_config_t){
								 .display	= config->display,
								 .gfx		= &surface->gfx,
								 .surface	= config->surface,
								 .api_switching = config->api_switching,
							 },
							 alloc);
	if (surface->backend == NULL) {
		gfx_free(&surface->gfx);
		*surface = (surface_t){0};
		return 1;
	}

	return 0;
}

int surface_config_window(surface_t *surface, window_config_t *config)
{
	if (surface == NULL) {
		return 1;
	}
	return surface_backend_config_window(surface->backend, config);
}

int surface_bind(surface_t *surface, window_t *window)
{
	if (surface == NULL || surface->backend == NULL || !surface_config_valid(&surface->config)) {
		return 1;
	}
	if (surface_backend_bind(surface->backend, window)) {
		return 1;
	}
	if (surface->backend->drv->gfx_init_order != SURFACE_GFX_INIT_AFTER_BIND || surface->gfx.drv != NULL) {
		return 0;
	}

	surface_native_t native = {0};
	if (surface_backend_native(surface->backend, &native) || native.gfx_surface == NULL) {
		surface_backend_unbind(surface->backend);
		return 1;
	}

	surface_plan_t plan = {0};
	if (surface_plan(&plan, &(surface_plan_config_t){.display = surface->config.display, .gfx_api = surface->config.driver->api})) {
		surface_backend_unbind(surface->backend);
		return 1;
	}
	if (gfx_init(&surface->gfx,
		     surface->config.driver,
		     &(gfx_config_t){
			     .plan    = &plan.gfx,
			     .surface = native.gfx_surface,
		     },
		     surface->proc,
		     surface->alloc) == NULL) {
		surface_backend_unbind(surface->backend);
		return 1;
	}

	surface->backend->config.gfx = &surface->gfx;
	return 0;
}

int surface_unbind(surface_t *surface)
{
	if (surface == NULL) {
		return 1;
	}
	return surface_backend_unbind(surface->backend);
}

int surface_native(surface_t *surface, surface_native_t *native)
{
	if (surface == NULL) {
		return 1;
	}
	return surface_backend_native(surface->backend, native);
}

gfx_t *surface_gfx(surface_t *surface)
{
	if (surface == NULL || surface->gfx.drv == NULL) {
		return NULL;
	}
	return &surface->gfx;
}

void surface_free(surface_t *surface)
{
	if (surface == NULL) {
		return;
	}

	if (surface->backend != NULL && surface->backend->drv != NULL &&
	    surface->backend->drv->gfx_init_order == SURFACE_GFX_INIT_AFTER_BIND) {
		gfx_free(&surface->gfx);
		surface_backend_destroy(surface->backend);
	} else {
		surface_backend_destroy(surface->backend);
		gfx_free(&surface->gfx);
	}
	*surface = (surface_t){0};
}

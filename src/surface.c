#include "surface.h"

#include "gfx_driver.h"
#include "surface_driver.h"

static int surface_config_valid(const surface_config_t *config)
{
	return config != NULL && config->display != NULL && (config->gfx != NULL || config->gfx_api != GFX_API_NONE);
}

static int surface_plan_config_valid(const surface_plan_config_t *config)
{
	return config != NULL && config->display != NULL;
}

static int surface_gfx_config_valid(const surface_gfx_config_t *config)
{
	return config != NULL && config->display != NULL && config->proc != NULL && config->driver != NULL &&
	       config->driver->api != GFX_API_NONE;
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

static int surface_info(const surface_config_t *config, surface_info_t *info)
{
	gfx_native_t native_gfx = {0};
	if (!surface_config_valid(config) || info == NULL) {
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

int surface_gfx_supported(const surface_gfx_config_t *config)
{
	if (!surface_gfx_config_valid(config)) {
		return 0;
	}

	surface_info_t info = {0};
	surface_info_display(config->display, config->driver->api, &info);

	surface_driver_t *drv = surface_driver_compatible(&info);
	if (drv == NULL) {
		return 0;
	}
	if (drv->plan == NULL) {
		return 1;
	}

	surface_plan_t plan = {0};
	return drv->plan(&info, &plan) == 0;
}

surface_t *surface_init_driver(surface_t *srf, const surface_driver_t *drv, const surface_config_t *config)
{
	if (srf == NULL || drv == NULL || drv->init == NULL || !surface_config_valid(config)) {
		return NULL;
	}

	srf->drv    = drv;
	srf->config = *config;
	if (srf->drv->init(srf, config)) {
		srf->drv    = NULL;
		srf->config = (surface_config_t){0};
		srf->data   = NULL;
		return NULL;
	}

	return srf;
}

surface_t *surface_init(surface_t *srf, const surface_config_t *config)
{
	if (srf == NULL || !surface_config_valid(config)) {
		return NULL;
	}

	surface_info_t info = {0};
	if (surface_info(config, &info)) {
		return NULL;
	}

	surface_driver_t *drv = surface_driver_compatible(&info);
	if (drv == NULL) {
		return NULL;
	}
	if (surface_init_driver(srf, drv, config) != NULL) {
		return srf;
	}

	return NULL;
}

int surface_gfx_init(surface_t *srf, gfx_t *gfx, const surface_gfx_config_t *config)
{
	if (srf == NULL || gfx == NULL || !surface_gfx_config_valid(config)) {
		return 1;
	}

	surface_info_t info = {0};
	surface_info_display(config->display, config->driver->api, &info);

	surface_driver_t *drv = surface_driver_compatible(&info);
	if (drv == NULL) {
		return 1;
	}

	surface_plan_t plan = {0};
	if (drv->plan != NULL && drv->plan(&info, &plan)) {
		return 1;
	}

	if (drv->gfx_init_order == SURFACE_GFX_INIT_AFTER_BIND) {
		return surface_init_driver(srf,
					   drv,
					   &(surface_config_t){
						   .display = config->display,
						   .gfx_api = config->driver->api,
						   .alloc   = config->alloc,
					   }) == NULL;
	}

	if (gfx_init(gfx,
		     config->driver,
		     &(gfx_config_t){
			     .proc  = config->proc,
			     .alloc = config->alloc,
			     .plan  = &plan.gfx,
		     }) == NULL) {
		return 1;
	}
	if (surface_init_driver(srf,
				drv,
				&(surface_config_t){
					.display = config->display,
					.gfx	 = gfx,
					.alloc	 = config->alloc,
				}) == NULL) {
		gfx_free(gfx);
		return 1;
	}

	return 0;
}

int surface_gfx_bind(surface_t *srf, gfx_t *gfx, window_t *window, const surface_gfx_config_t *config)
{
	if (srf == NULL || srf->drv == NULL || gfx == NULL || !surface_gfx_config_valid(config)) {
		return 1;
	}
	if (surface_bind(srf, window)) {
		return 1;
	}
	if (srf->drv->gfx_init_order != SURFACE_GFX_INIT_AFTER_BIND || gfx->drv != NULL) {
		return 0;
	}

	surface_native_t native = {0};
	if (surface_native(srf, &native) || native.gfx_surface == NULL) {
		surface_unbind(srf);
		return 1;
	}

	surface_plan_t plan = {0};
	if (surface_plan(&plan, &(surface_plan_config_t){.display = config->display, .gfx_api = config->driver->api})) {
		surface_unbind(srf);
		return 1;
	}
	if (gfx_init(gfx,
		     config->driver,
		     &(gfx_config_t){
			     .proc    = config->proc,
			     .alloc   = config->alloc,
			     .plan    = &plan.gfx,
			     .surface = native.gfx_surface,
		     }) == NULL) {
		surface_unbind(srf);
		return 1;
	}

	srf->config.gfx = gfx;
	return 0;
}

void surface_free(surface_t *srf)
{
	if (srf == NULL || srf->drv == NULL) {
		return;
	}

	if (srf->drv->free != NULL) {
		srf->drv->free(srf);
	}
	srf->drv    = NULL;
	srf->config = (surface_config_t){0};
	srf->data   = NULL;
}

int surface_config_window(surface_t *srf, window_config_t *config)
{
	if (srf == NULL || srf->drv == NULL || srf->drv->config_window == NULL || config == NULL) {
		return 1;
	}

	return srf->drv->config_window(srf, config);
}

int surface_bind(surface_t *srf, window_t *window)
{
	if (srf == NULL || srf->drv == NULL || srf->drv->bind == NULL || window == NULL) {
		return 1;
	}

	return srf->drv->bind(srf, window);
}

int surface_unbind(surface_t *srf)
{
	if (srf == NULL || srf->drv == NULL || srf->drv->unbind == NULL) {
		return 1;
	}

	return srf->drv->unbind(srf);
}

int surface_native(surface_t *srf, surface_native_t *native)
{
	if (srf == NULL || srf->drv == NULL || srf->drv->native == NULL || native == NULL) {
		return 1;
	}

	return srf->drv->native(srf, native);
}

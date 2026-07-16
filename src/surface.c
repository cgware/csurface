#include "surface.h"

#include "surface_driver.h"

static int surface_config_valid(const surface_config_t *config)
{
	return config != NULL && config->display != NULL && config->gfx != NULL;
}

static int surface_plan_config_valid(const surface_plan_config_t *config)
{
	return config != NULL && config->display != NULL;
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
	if (!surface_config_valid(config) || info == NULL || gfx_native(config->gfx, &native_gfx)) {
		return 1;
	}

	surface_info_display(config->display, native_gfx.api, info);
	return 0;
}

int surface_plan(surface_plan_t *plan, const surface_plan_config_t *config)
{
	if (plan == NULL || !surface_plan_config_valid(config)) {
		return 1;
	}

	surface_info_t info = {0};
	surface_info_display(config->display, config->gfx_api, &info);

	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != SURFACE_DRIVER_TYPE) {
			continue;
		}

		surface_driver_t *drv = i->data;
		if (drv == NULL || drv->compatible == NULL || !drv->compatible(&info)) {
			continue;
		}
		if (drv->plan == NULL) {
			return 0;
		}
		return drv->plan(&info, plan);
	}

	return 1;
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

	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type != SURFACE_DRIVER_TYPE) {
			continue;
		}

		surface_driver_t *drv = i->data;
		if (drv == NULL || drv->compatible == NULL || !drv->compatible(&info)) {
			continue;
		}
		if (surface_init_driver(srf, drv, config) != NULL) {
			return srf;
		}
	}

	return NULL;
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

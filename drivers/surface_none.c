#include "surface_driver.h"

#include <stddef.h>

static int surface_none_compatible(const surface_info_t *info)
{
	return info != NULL && info->gfx_api == GFX_API_NONE && info->native_type == DISPLAY_NATIVE_NONE;
}

static int surface_none_init(surface_t *srf, const surface_config_t *config)
{
	if (srf == NULL || config == NULL) {
		return 1;
	}

	return 0;
}

static int surface_none_free(surface_t *srf)
{
	if (srf == NULL) {
		return 1;
	}

	return 0;
}

static surface_driver_t surface_none = {
	.name	    = "none",
	.compatible = surface_none_compatible,
	.init	    = surface_none_init,
	.free	    = surface_none_free,
};

SURFACE_DRIVER(surface_none, &surface_none);

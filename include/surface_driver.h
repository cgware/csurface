#ifndef SURFACE_DRIVER_H
#define SURFACE_DRIVER_H

#include "driver.h"
#include "surface.h"

typedef struct surface_driver_s {
	const char *name;
	int (*compatible)(const surface_info_t *info);
	int (*init)(surface_t *srf, const surface_config_t *config);
	int (*free)(surface_t *srf);
	int (*config_window)(surface_t *srf, window_config_t *config);
	int (*bind)(surface_t *srf, window_t *window);
	int (*unbind)(surface_t *srf);
	int (*present)(surface_t *srf);
} surface_driver_t;

surface_t *surface_init_driver(surface_t *srf, const surface_driver_t *drv, const surface_config_t *config);

#define SURFACE_DRIVER_TYPE 0x535246

#define SURFACE_DRIVER(_name, _data) DRIVER(_name, SURFACE_DRIVER_TYPE, _data)

#endif

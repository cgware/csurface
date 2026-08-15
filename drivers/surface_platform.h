#ifndef SURFACE_PLATFORM_H
#define SURFACE_PLATFORM_H

#include "surface_driver.h"

typedef struct surface_platform_library_s {
	strv_t name;
	void **handle;
} surface_platform_library_t;

void *surface_platform_alloc(surface_backend_t *srf, const surface_backend_config_t *config, size_t size, const char *driver,
			     int require_proc);
void surface_platform_free(surface_backend_t *srf, size_t size);
void surface_platform_default_window_config(window_config_t *config);
int surface_platform_display_native(surface_backend_t *srf, display_native_type_t type, display_native_t *native, const char *driver,
				    const char *label);
int surface_platform_window_native(window_t *window, display_native_type_t type, window_native_t *native, const char *driver,
				   const char *label);
int surface_platform_load_library(proc_t *proc, const strv_t *names, size_t name_count, void **lib, const char *driver, const char *label);
int surface_platform_load_libraries(proc_t *proc, const surface_platform_library_t *libraries, size_t library_count, const char *driver);
int surface_platform_load_symbol(proc_t *proc, void *lib, void **sym, strv_t name, const char *driver, const char *label);
void surface_platform_close_library_set(proc_t *proc, const surface_platform_library_t *libraries, size_t library_count);
void surface_platform_gfx_surface(gfx_surface_t *surface, gfx_api_t api, u64 handle, void *data, const gfx_surface_ops_t *ops);
void surface_platform_native(surface_native_t *native, gfx_api_t gfx_api, display_native_type_t native_type, void *display, void *visual,
			     u64 handle, gfx_surface_t *gfx_surface);

#endif

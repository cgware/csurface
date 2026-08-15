#include "surface_platform.h"

#include "log.h"
#include "mem.h"

void *surface_platform_alloc(surface_backend_t *srf, const surface_backend_config_t *config, size_t size, const char *driver,
			     int require_proc)
{
	if (srf == NULL || config == NULL || (require_proc && (config->display == NULL || config->display->proc == NULL))) {
		return NULL;
	}

	void *ctx = alloc_alloc(&srf->alloc, size);
	if (ctx == NULL) {
		log_error("csurface", driver, NULL, "failed to allocate surface data");
		return NULL;
	}
	mem_set(ctx, 0, size);
	return ctx;
}

void surface_platform_free(surface_backend_t *srf, size_t size)
{
	if (srf == NULL || srf->data == NULL) {
		return;
	}

	alloc_free(&srf->alloc, srf->data, size);
	srf->data = NULL;
}

void surface_platform_default_window_config(window_config_t *config)
{
	config->depth	   = 0;
	config->visual	   = 0;
	config->background = WINDOW_BACKGROUND_NONE;
}

int surface_platform_display_native(surface_backend_t *srf, display_native_type_t type, display_native_t *native, const char *driver,
				    const char *label)
{
	if (display_native(srf->config.display, native) || native->type != type || native->display == NULL) {
		log_error("csurface", driver, NULL, "%s native display is unavailable", label);
		return 1;
	}

	return 0;
}

int surface_platform_window_native(window_t *window, display_native_type_t type, window_native_t *native, const char *driver,
				   const char *label)
{
	if (window_native(window, native) || native->type != type || native->window == NULL) {
		log_error("csurface", driver, NULL, "%s native window is unavailable", label);
		return 1;
	}

	return 0;
}

int surface_platform_load_library(proc_t *proc, const strv_t *names, size_t name_count, void **lib, const char *driver, const char *label)
{
	for (size_t i = 0; i < name_count; i++) {
		if (!proc_dlopen(proc, names[i], lib)) {
			return 0;
		}
	}

	log_error("csurface", driver, NULL, "failed to load %s", label);
	return 1;
}

int surface_platform_load_libraries(proc_t *proc, const surface_platform_library_t *libraries, size_t library_count, const char *driver)
{
	for (size_t i = 0; i < library_count; i++) {
		if (proc_dlopen(proc, libraries[i].name, libraries[i].handle)) {
			log_error("csurface", driver, NULL, "failed to load %.*s", libraries[i].name.len, libraries[i].name.data);
			while (i > 0) {
				i--;
				if (*libraries[i].handle != NULL) {
					proc_dlclose(proc, *libraries[i].handle);
					*libraries[i].handle = NULL;
				}
			}
			return 1;
		}
	}

	return 0;
}

int surface_platform_load_symbol(proc_t *proc, void *lib, void **sym, strv_t name, const char *driver, const char *label)
{
	if (proc_dlsym(proc, lib, name, sym)) {
		log_error("csurface", driver, NULL, "failed to load %s symbol: %.*s", label, name.len, name.data);
		return 1;
	}

	return 0;
}

void surface_platform_close_library_set(proc_t *proc, const surface_platform_library_t *libraries, size_t library_count)
{
	for (size_t i = 0; i < library_count; i++) {
		if (*libraries[i].handle != NULL) {
			proc_dlclose(proc, *libraries[i].handle);
			*libraries[i].handle = NULL;
		}
	}
}

void surface_platform_gfx_surface(gfx_surface_t *surface, gfx_api_t api, u64 handle, void *data, const gfx_surface_ops_t *ops)
{
	*surface = (gfx_surface_t){
		.api	= api,
		.handle = handle,
		.data	= data,
		.ops	= ops,
	};
}

void surface_platform_native(surface_native_t *native, gfx_api_t gfx_api, display_native_type_t native_type, void *display, void *visual,
			     u64 handle, gfx_surface_t *gfx_surface)
{
	*native = (surface_native_t){
		.gfx_api     = gfx_api,
		.native_type = native_type,
		.display     = display,
		.visual	     = visual,
		.handle	     = handle,
		.gfx_surface = gfx_surface,
	};
}

#include "display_driver.h"
#include "gfx_driver.h"
#include "log.h"
#include "monitor.h"
#include "surface.h"

enum {
	EXAMPLE_MAX_TARGETS  = 8,
	EXAMPLE_MAX_DISPLAYS = 8,
};

typedef struct example_target_s {
	gfx_driver_t *driver;
	gfx_t gfx;
	surface_t surface;
	window_t window;
	u32 id;
	u16 width;
	u16 height;
	int open;
	int initialized;
} example_target_t;

typedef struct example_state_s {
	example_target_t *targets;
	u32 count;
	u32 open;
	proc_t *proc;
	display_t *display;
	int failed;
} example_state_t;

static int draw(example_target_t *target)
{
	if (target == NULL) {
		return 1;
	}

	gfx_t *gfx		    = &target->gfx;
	gfx_vertex_2d_t vertices[3] = {
		{
			.x = (float)target->width * 0.5f,
			.y = (float)target->height * 0.15f,
			.r = 1.0f,
			.a = 1.0f,
		},
		{
			.x = (float)target->width * 0.85f,
			.y = (float)target->height * 0.85f,
			.g = 1.0f,
			.a = 1.0f,
		},
		{
			.x = (float)target->width * 0.15f,
			.y = (float)target->height * 0.85f,
			.b = 1.0f,
			.a = 1.0f,
		},
	};
	if (gfx_clear_color(gfx, 0.1f, 0.2f, 0.3f, 1.0f)) {
		log_error("csurface_example", "draw", NULL, "failed to set clear color");
		return 1;
	}
	if (gfx_viewport(gfx, 0, 0, target->width, target->height)) {
		log_error("csurface_example", "draw", NULL, "failed to set viewport");
		return 1;
	}
	if (gfx_clear(gfx, GFX_CLEAR_COLOR_BUFFER)) {
		log_error("csurface_example", "draw", NULL, "failed to clear color buffer");
		return 1;
	}
	if (target->driver->draw_triangle_2d != NULL && gfx_draw_triangle_2d(gfx, vertices)) {
		log_error("csurface_example", "draw", NULL, "failed to draw triangle");
		return 1;
	}
	if (gfx_present(gfx)) {
		log_error("csurface_example", "draw", NULL, "failed to present frame");
		return 1;
	}

	return 0;
}

static int window_position(u16 *position, s32 origin, u32 offset)
{
	s64 value = (s64)origin + offset;
	if (position == NULL || value < U16_MIN || value > U16_MAX) {
		return 1;
	}

	*position = (u16)value;
	return 0;
}

static int print_monitors(display_t *display, const char *driver_name, display_monitor_t *show_monitor, int *has_monitor)
{
	arr_t monitors = {0};
	if (has_monitor != NULL) {
		*has_monitor = 0;
	}
	if (arr_init(&monitors, 1, sizeof(display_monitor_t), ALLOC_STD) == NULL) {
		return 1;
	}
	if (display_monitors(display, &monitors)) {
		arr_free(&monitors);
		return 1;
	}

	dputf(DST_STD(), "%s monitors:\n", driver_name);
	for (u32 i = 0; i < monitors.cnt; i++) {
		monitor_print(arr_get(&monitors, i), DST_STD());
	}
	if (monitors.cnt > 0 && show_monitor != NULL && has_monitor != NULL) {
		display_monitor_t *monitor = arr_get(&monitors, 2 >= monitors.cnt ? 0 : 2);
		*show_monitor		   = *monitor;
		*has_monitor		   = 1;
	}

	arr_free(&monitors);
	return 0;
}

static int draw_all(example_target_t *targets, u32 count)
{
	for (u32 i = 0; i < count; i++) {
		if (!targets[i].open) {
			continue;
		}
		if (draw(&targets[i])) {
			log_error("csurface_example", "draw", NULL, "failed to draw with graphics driver: %s", targets[i].driver->name);
			return 1;
		}
	}

	return 0;
}

static int set_target_size(example_target_t *target, u16 width, u16 height)
{
	if (target == NULL || width == 0 || height == 0) {
		return 1;
	}

	surface_native_t native = {0};
	if (surface_native(&target->surface, &native)) {
		return 1;
	}
	gfx_target_t gfx_target = {
		.type	 = GFX_TARGET_SURFACE,
		.format	 = GFX_FORMAT_RGBA8,
		.surface = native.gfx_surface,
		.width	 = width,
		.height	 = height,
	};
	if (gfx_set_target(&target->gfx, &gfx_target)) {
		return 1;
	}

	target->width  = width;
	target->height = height;
	return 0;
}

static void clear_target_graphics(example_target_t *target)
{
	if (target == NULL) {
		return;
	}

	gfx_target_t gfx_target = {.type = GFX_TARGET_NONE};
	gfx_set_target(&target->gfx, &gfx_target);
	surface_free(&target->surface);
	gfx_free(&target->gfx);
	target->driver = NULL;
}

static surface_gfx_config_t target_graphics_config(display_t *display, proc_t *proc, gfx_driver_t *driver)
{
	return (surface_gfx_config_t){
		.display = display,
		.proc	 = proc,
		.driver	 = driver,
		.alloc	 = ALLOC_STD,
	};
}

static int init_target_graphics(display_t *display, proc_t *proc, gfx_driver_t *driver, example_target_t *target)
{
	if (display == NULL || proc == NULL || driver == NULL || target == NULL) {
		return -1;
	}

	surface_gfx_config_t config = target_graphics_config(display, proc, driver);
	if (!surface_gfx_supported(&config)) {
		return 0;
	}

	target->driver = driver;
	if (surface_gfx_init(&target->surface, &target->gfx, &config)) {
		log_error("csurface_example", "init", NULL, "failed to initialize surface graphics for driver: %s", driver->name);
		clear_target_graphics(target);
		return -1;
	}

	return 1;
}

static void destroy_target(example_target_t *target)
{
	if (target == NULL || !target->initialized) {
		return;
	}

	target->open = 0;
	clear_target_graphics(target);
	window_free(&target->window);
	target->initialized = 0;
}

static void close_all(example_state_t *state)
{
	if (state == NULL) {
		return;
	}

	for (u32 i = 0; i < state->count; i++) {
		destroy_target(&state->targets[i]);
	}
	state->open = 0;
}

static void destroy_closed(example_state_t *state)
{
	if (state == NULL) {
		return;
	}

	for (u32 i = 0; i < state->count; i++) {
		if (!state->targets[i].open) {
			destroy_target(&state->targets[i]);
		}
	}
}

static void free_graphics(example_target_t *targets, u32 count)
{
	for (u32 i = 0; i < count; i++) {
		clear_target_graphics(&targets[i]);
	}
}

static int fail_target_init(example_target_t *target)
{
	clear_target_graphics(target);
	window_free(&target->window);
	return -1;
}

static int bind_target_graphics(display_t *display, proc_t *proc, example_target_t *target, window_t *window)
{
	if (display == NULL || proc == NULL || target == NULL || target->driver == NULL || window == NULL) {
		return 1;
	}

	surface_gfx_config_t config = target_graphics_config(display, proc, target->driver);
	return surface_gfx_bind(&target->surface, &target->gfx, window, &config);
}

static int restore_target_graphics(example_state_t *state, example_target_t *target)
{
	if (state == NULL || target == NULL) {
		return 1;
	}
	if (bind_target_graphics(state->display, state->proc, target, &target->window)) {
		return 1;
	}
	return set_target_size(target, target->width, target->height);
}

static int switch_target_graphics(example_state_t *state, example_target_t *target, gfx_driver_t *driver)
{
	if (state == NULL || target == NULL || driver == NULL || target->driver == NULL || driver == target->driver) {
		return 0;
	}

	example_target_t next = {.width = target->width, .height = target->height};
	int initialized	      = init_target_graphics(state->display, state->proc, driver, &next);
	if (initialized <= 0) {
		return initialized;
	}

	window_config_t config = {
		.width	= target->width,
		.height = target->height,
	};
	if (surface_config_window(&next.surface, &config)) {
		clear_target_graphics(&next);
		return 0;
	}

	gfx_target_t gfx_target = {.type = GFX_TARGET_NONE};
	if (gfx_set_target(&target->gfx, &gfx_target) || surface_unbind(&target->surface)) {
		clear_target_graphics(&next);
		return -1;
	}
	if (bind_target_graphics(state->display, state->proc, &next, &target->window) ||
	    set_target_size(&next, target->width, target->height)) {
		clear_target_graphics(&next);
		if (restore_target_graphics(state, target)) {
			return -1;
		}
		return 0;
	}

	gfx_t old_gfx		   = target->gfx;
	surface_t old_surface	   = target->surface;
	old_surface.config.gfx	   = &old_gfx;
	next.surface.config.gfx	   = &next.gfx;
	target->gfx		   = next.gfx;
	target->surface		   = next.surface;
	target->surface.config.gfx = &target->gfx;
	target->driver		   = driver;
	surface_free(&old_surface);
	gfx_free(&old_gfx);

	if (window_set_title(&target->window, strv_cstr(driver->name))) {
		return -1;
	}
	return 1;
}

static int switch_target_next_graphics(example_state_t *state, example_target_t *target)
{
	if (state == NULL || target == NULL || target->driver == NULL) {
		return 0;
	}

	for (gfx_driver_t *driver = gfx_driver_next(target->driver); driver != NULL && driver != target->driver;
	     driver		  = gfx_driver_next(driver)) {
		int switched = switch_target_graphics(state, target, driver);
		if (switched != 0) {
			return switched;
		}
	}

	return 0;
}

static example_target_t *find_target(example_target_t *targets, u32 count, u32 id)
{
	for (u32 i = 0; i < count; i++) {
		if (targets[i].id == id) {
			return &targets[i];
		}
	}

	return NULL;
}

static void on_event(display_t *display, const display_event_t *event, void *user)
{
	(void)display;

	example_state_t *state = user;
	if (state == NULL || event == NULL) {
		return;
	}

	example_target_t *target = find_target(state->targets, state->count, event->window);
	if (target == NULL) {
		return;
	}

	switch (event->type) {
	case DISPLAY_EVENT_CLOSE:
		if (target->open && state->open > 0) {
			target->open = 0;
			state->open--;
		}
		return;
	case DISPLAY_EVENT_KEY_DOWN:
		switch (event->key) {
		case DISPLAY_KEY_ESCAPE:
			if (target->open && state->open > 0) {
				target->open = 0;
				state->open--;
			}
			return;
		case DISPLAY_KEY_F11:
			if (!target->open) {
				return;
			}

			int fullscreen = 0;
			if (window_get_fullscreen(&target->window, &fullscreen)) {
				log_error("csurface_example",
					  "event",
					  NULL,
					  "failed to get fullscreen state for driver: %s",
					  target->driver->name);
				state->failed = 1;
				return;
			}
			fullscreen = !fullscreen;
			if (window_set_fullscreen(&target->window, fullscreen)) {
				log_error("csurface_example",
					  "event",
					  NULL,
					  "failed to toggle fullscreen for driver: %s",
					  target->driver->name);
				state->failed = 1;
				return;
			}
			return;
		case DISPLAY_KEY_F1:
			if (!target->open) {
				return;
			}

			int switched = switch_target_next_graphics(state, target);
			if (switched < 0) {
				log_error("csurface_example", "event", NULL, "failed to switch graphics driver for window: %u", target->id);
				state->failed = 1;
			}
			return;
		default:
			break;
		}
		break;
	case DISPLAY_EVENT_RESIZE:
		if (!target->open) {
			return;
		}
		if (event->width == target->width && event->height == target->height) {
			return;
		}
		if (set_target_size(target, event->width, event->height)) {
			log_error(
				"csurface_example", "event", NULL, "failed to resize graphics target for driver: %s", target->driver->name);
			state->failed = 1;
		}
		break;
	default:
		break;
	}
}

static int open_target(display_t *display, proc_t *proc, gfx_driver_t *driver, const display_monitor_t *monitor, u32 index,
		       example_target_t *target)
{
	u16 x = 0;
	u16 y = 0;
	if (window_position(&x, monitor != NULL ? monitor->x : 0, 100 + index * 40) ||
	    window_position(&y, monitor != NULL ? monitor->y : 0, 100 + index * 40)) {
		log_error("csurface_example", "init", NULL, "failed to place window for graphics driver: %s", driver->name);
		return -1;
	}

	window_config_t config = {
		.x	= x,
		.y	= y,
		.width	= 640,
		.height = 480,
	};

	int initialized = init_target_graphics(display, proc, driver, target);
	if (initialized <= 0) {
		if (initialized == 0) {
			return 0;
		}
		return fail_target_init(target);
	}
	if (surface_config_window(&target->surface, &config)) {
		log_error("csurface_example", "init", NULL, "failed to configure window for graphics driver: %s", driver->name);
		return fail_target_init(target);
	}
	if (window_init(&target->window, display, &config) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize window for graphics driver: %s", driver->name);
		return fail_target_init(target);
	}
	if (window_set_title(&target->window, strv_cstr(driver->name))) {
		log_error("csurface_example", "init", NULL, "failed to set window title for graphics driver: %s", driver->name);
		return fail_target_init(target);
	}
	if (window_show(&target->window)) {
		log_error("csurface_example", "init", NULL, "failed to show window for graphics driver: %s", driver->name);
		return fail_target_init(target);
	}
	if (bind_target_graphics(display, proc, target, &target->window)) {
		log_error("csurface_example", "init", NULL, "failed to bind surface for graphics driver: %s", driver->name);
		return fail_target_init(target);
	}
	if (set_target_size(target, config.width, config.height)) {
		log_error("csurface_example", "init", NULL, "failed to set surface target for graphics driver: %s", driver->name);
		return fail_target_init(target);
	}
	target->id	    = window_id(&target->window);
	target->open	    = 1;
	target->initialized = 1;

	return 1;
}

static int run_display_driver(display_driver_t *display_driver, fs_t *fs, proc_t *proc, sock_t *sock)
{
	display_t display			      = {0};
	gfx_driver_t *drivers[EXAMPLE_MAX_TARGETS]    = {0};
	example_target_t targets[EXAMPLE_MAX_TARGETS] = {0};
	u32 target_count			      = 0;
	int ret					      = 0;

	if (display_driver == NULL || display_driver->native == NULL) {
		return 0;
	}

	if (display_init(&display, display_driver, fs, proc, sock, ALLOC_STD) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize display driver: %s", display_driver->name);
		return 1;
	}
	display_monitor_t show_monitor = {0};
	int has_monitor		       = 0;
	if (print_monitors(&display, display_driver->name, &show_monitor, &has_monitor)) {
		log_error("csurface_example", "init", NULL, "failed to list monitors for display driver: %s", display_driver->name);
		display_free(&display);
		return 1;
	}

	u32 driver_count = gfx_driver_list(drivers, sizeof(drivers) / sizeof(drivers[0]));
	if (driver_count > sizeof(drivers) / sizeof(drivers[0])) {
		driver_count = sizeof(drivers) / sizeof(drivers[0]);
	}
	for (u32 i = 0; i < driver_count; i++) {
		if (drivers[i] == NULL) {
			continue;
		}
		int opened =
			open_target(&display, proc, drivers[i], has_monitor ? &show_monitor : NULL, target_count, &targets[target_count]);
		if (opened < 0) {
			if (targets[target_count].gfx.drv != NULL) {
				target_count++;
			}
			ret = 1;
			break;
		}
		if (opened > 0) {
			target_count++;
		}
	}
	if (ret == 0 && target_count == 0) {
		log_error("csurface_example",
			  "init",
			  NULL,
			  "no graphics drivers are compatible with display driver: %s",
			  display_driver->name);
		ret = 1;
	}

	example_state_t state = {
		.targets = targets,
		.count	 = target_count,
		.open	 = target_count,
		.proc	 = proc,
		.display = &display,
	};
	if (ret == 0) {
		display_set_event_callback(&display, on_event, &state);
		if (draw_all(targets, target_count)) {
			log_error(
				"csurface_example", "draw", NULL, "initial frame draw failed for display driver: %s", display_driver->name);
			ret = 1;
		}
	}
	while (ret == 0) {
		if (display_wait_events(&display)) {
			log_error("csurface_example",
				  "event",
				  NULL,
				  "failed to wait for display events from driver: %s",
				  display_driver->name);
			ret = 1;
			break;
		}
		if (state.failed) {
			ret = 1;
			break;
		}
		destroy_closed(&state);
		if (state.open == 0) {
			break;
		}
		if (draw_all(targets, target_count)) {
			log_error("csurface_example", "draw", NULL, "frame draw failed for display driver: %s", display_driver->name);
			ret = 1;
		}
	}

	close_all(&state);
	free_graphics(targets, target_count);
	display_free(&display);
	return ret;
}

int main(void)
{
	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_ERROR, 1, 1);

	fs_t fs						= {0};
	proc_t proc					= {0};
	sock_t sock					= {0};
	display_driver_t *drivers[EXAMPLE_MAX_DISPLAYS] = {0};
	int ret						= 0;

	fs_init(&fs, 0, 0, ALLOC_STD);
	proc_init(&proc, 0, 0, ALLOC_STD);
	sock_init(&sock, 0, 0, ALLOC_STD);

	u32 driver_count = display_driver_list(drivers, sizeof(drivers) / sizeof(drivers[0]));
	if (driver_count > sizeof(drivers) / sizeof(drivers[0])) {
		driver_count = sizeof(drivers) / sizeof(drivers[0]);
	}
	if (driver_count == 0) {
		log_error("csurface_example", "init", NULL, "no display drivers found");
		ret = 1;
	} else {
		u32 tried = 0;
		for (u32 i = 0; i < driver_count; i++) {
			if (drivers[i] == NULL || drivers[i]->native == NULL) {
				continue;
			}
			if (!display_driver_available(drivers[i], &proc)) {
				continue;
			}
			tried++;
			if (run_display_driver(drivers[i], &fs, &proc, &sock)) {
				ret = 1;
				break;
			}
		}
		if (tried == 0) {
			log_error("csurface_example", "init", NULL, "no native display drivers found");
			ret = 1;
		}
	}

	sock_free(&sock);
	proc_free(&proc);
	fs_free(&fs);
	return ret;
}

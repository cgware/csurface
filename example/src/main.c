#include "display_driver.h"
#include "gfx_driver.h"
#include "log.h"
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
	int failed;
} example_state_t;

static int draw(gfx_t *gfx)
{
	if (gfx_clear_color(gfx, 0.1f, 0.2f, 0.3f, 1.0f)) {
		log_error("csurface_example", "draw", NULL, "failed to set clear color");
		return 1;
	}
	if (gfx_clear(gfx, GFX_CLEAR_COLOR_BUFFER)) {
		log_error("csurface_example", "draw", NULL, "failed to clear color buffer");
		return 1;
	}
	if (gfx_present(gfx)) {
		log_error("csurface_example", "draw", NULL, "failed to present frame");
		return 1;
	}

	return 0;
}

static int draw_all(example_target_t *targets, u32 count)
{
	for (u32 i = 0; i < count; i++) {
		if (!targets[i].open) {
			continue;
		}
		if (draw(&targets[i].gfx)) {
			log_error("csurface_example", "draw", NULL, "failed to draw with graphics driver: %s", targets[i].driver->name);
			return 1;
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

static void destroy_target(example_target_t *target)
{
	if (target == NULL || !target->initialized) {
		return;
	}

	target->open		= 0;
	gfx_target_t gfx_target = {.type = GFX_TARGET_NONE};
	gfx_set_target(&target->gfx, &gfx_target);
	surface_free(&target->surface);
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
		gfx_free(&targets[i].gfx);
	}
}

static int fail_target_init(example_target_t *target)
{
	gfx_target_t gfx_target = {.type = GFX_TARGET_NONE};
	gfx_set_target(&target->gfx, &gfx_target);
	surface_free(&target->surface);
	window_free(&target->window);
	return -1;
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

	if (event->type == DISPLAY_EVENT_CLOSE || (event->type == DISPLAY_EVENT_KEY_DOWN && event->key == DISPLAY_KEY_ESCAPE)) {
		if (target->open && state->open > 0) {
			target->open = 0;
			state->open--;
		}
		return;
	}
	if (event->type == DISPLAY_EVENT_RESIZE && target->open) {
		if (event->width == target->width && event->height == target->height) {
			return;
		}
		if (set_target_size(target, event->width, event->height)) {
			log_error(
				"csurface_example", "event", NULL, "failed to resize graphics target for driver: %s", target->driver->name);
			state->failed = 1;
		}
	}
}

static int open_target(display_t *display, proc_t *proc, gfx_driver_t *driver, u32 index, example_target_t *target)
{
	surface_plan_t plan    = {0};
	window_config_t config = {
		.x	= (u16)(100 + index * 40),
		.y	= (u16)(100 + index * 40),
		.width	= 640,
		.height = 480,
	};

	if (surface_plan(&plan, &(surface_plan_config_t){.display = display, .gfx_api = driver->api})) {
		return 0;
	}

	target->driver = driver;
	if (gfx_init(&target->gfx, driver, &(gfx_config_t){.proc = proc, .alloc = ALLOC_STD, .plan = &plan.gfx}) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize graphics driver: %s", driver->name);
		return fail_target_init(target);
	}
	if (surface_init(&target->surface,
			 &(surface_config_t){
				 .display = display,
				 .gfx	  = &target->gfx,
				 .alloc	  = ALLOC_STD,
			 }) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize surface for graphics driver: %s", driver->name);
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
	if (surface_bind(&target->surface, &target->window)) {
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

	u32 driver_count = gfx_driver_list(drivers, sizeof(drivers) / sizeof(drivers[0]));
	if (driver_count > sizeof(drivers) / sizeof(drivers[0])) {
		driver_count = sizeof(drivers) / sizeof(drivers[0]);
	}
	for (u32 i = 0; i < driver_count; i++) {
		if (drivers[i] == NULL) {
			continue;
		}
		int opened = open_target(&display, proc, drivers[i], target_count, &targets[target_count]);
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

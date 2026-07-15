#include "display_driver.h"
#include "gfx_driver.h"
#include "log.h"
#include "surface.h"

static display_driver_t *find_display_driver(strv_t name)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type == DISPLAY_DRIVER_TYPE) {
			display_driver_t *drv = i->data;
			if (strv_eq(strv_cstr(drv->name), name)) {
				return drv;
			}
		}
	}

	return NULL;
}

static gfx_driver_t *find_gfx_driver(strv_t name)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type == GFX_DRIVER_TYPE) {
			gfx_driver_t *drv = i->data;
			if (strv_eq(strv_cstr(drv->name), name)) {
				return drv;
			}
		}
	}

	return NULL;
}

static int draw(surface_t *surface, gfx_t *gfx)
{
	if (gfx_clear_color(gfx, 0.1f, 0.2f, 0.3f, 1.0f)) {
		log_error("csurface_example", "draw", NULL, "failed to set clear color");
		return 1;
	}
	if (gfx_clear(gfx, GFX_CLEAR_COLOR_BUFFER)) {
		log_error("csurface_example", "draw", NULL, "failed to clear color buffer");
		return 1;
	}

	return surface_present(surface);
}

static void on_event(display_t *display, const display_event_t *event, void *user)
{
	(void)display;

	int *running = user;
	if (event->type == DISPLAY_EVENT_CLOSE || (event->type == DISPLAY_EVENT_KEY_DOWN && event->key == DISPLAY_KEY_ESCAPE)) {
		*running = 0;
	}
}

int main(void)
{
	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_ERROR, 1, 1);

	fs_t fs		  = {0};
	proc_t proc	  = {0};
	sock_t sock	  = {0};
	display_t display = {0};
	window_t window	  = {0};
	gfx_t gfx	  = {0};
	surface_t surface = {0};
	int ret		  = 0;

	fs_init(&fs, 0, 0, ALLOC_STD);
	proc_init(&proc, 0, 0, ALLOC_STD);
	sock_init(&sock, 0, 0, ALLOC_STD);

	window_config_t config	 = {.x = 100, .y = 100, .width = 640, .height = 480};
	display_driver_t *driver = find_display_driver(STRV("X11-dynamic"));
	gfx_driver_t *gfx_driver = find_gfx_driver(STRV("opengl"));
	if (driver == NULL) {
		log_error("csurface_example", "init", NULL, "display driver not found: X11-dynamic");
		ret = 1;
	} else if (gfx_driver == NULL) {
		log_error("csurface_example", "init", NULL, "graphics driver not found: opengl");
		ret = 1;
	} else if (display_init(&display, driver, &fs, &proc, &sock, ALLOC_STD) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize display");
		ret = 1;
	} else if (gfx_init(&gfx, gfx_driver, &(gfx_config_t){.proc = &proc, .alloc = ALLOC_STD}) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize graphics");
		ret = 1;
	} else if (surface_init(&surface,
				&(surface_config_t){
					.display = &display,
					.gfx	 = &gfx,
					.alloc	 = ALLOC_STD,
				}) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize surface");
		ret = 1;
	} else if (surface_config_window(&surface, &config)) {
		log_error("csurface_example", "init", NULL, "failed to configure window for surface");
		ret = 1;
	} else if (window_init(&window, &display, &config) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize window");
		ret = 1;
	} else if (window_set_title(&window, STRV("OpenGL"))) {
		log_error("csurface_example", "init", NULL, "failed to set window title");
		ret = 1;
	} else if (window_show(&window)) {
		log_error("csurface_example", "init", NULL, "failed to show window");
		ret = 1;
	} else if (surface_bind(&surface, &window)) {
		log_error("csurface_example", "init", NULL, "failed to bind surface to window");
		ret = 1;
	}

	int running = 1;
	if (ret == 0) {
		display_set_event_callback(&display, on_event, &running);
	}
	if (ret == 0 && draw(&surface, &gfx)) {
		log_error("csurface_example", "draw", NULL, "initial draw failed");
		ret = 1;
	}
	while (ret == 0 && running) {
		if (display_wait_events(&display)) {
			log_error("csurface_example", "event", NULL, "failed to wait for display events");
			ret = 1;
			break;
		}
		if (!running) {
			break;
		}
		if (draw(&surface, &gfx)) {
			log_error("csurface_example", "draw", NULL, "frame draw failed");
			ret = 1;
		}
	}

	surface_free(&surface);
	window_free(&window);
	display_free(&display);
	gfx_free(&gfx);
	sock_free(&sock);
	proc_free(&proc);
	fs_free(&fs);
	return ret;
}

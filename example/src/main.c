#include "cmath.h"
#include "ctime.h"
#include "display_driver.h"
#include "gfx_driver.h"
#include "log.h"
#include "mem.h"
#include "print.h"
#include "surface.h"

enum {
	EXAMPLE_MAX_TARGETS		= 8,
	EXAMPLE_MAX_DISPLAYS		= 8,
	EXAMPLE_FPS_INTERVAL		= 1000,
	EXAMPLE_SOFTWARE_FRAME_INTERVAL = 1000,
	EXAMPLE_TITLE_SIZE		= 128,
	EXAMPLE_RECT_INDEX_COUNT	= 6,
	EXAMPLE_CUBE_INDEX_COUNT	= 36,
	EXAMPLE_SWAPCHAIN_IMAGE_COUNT	= 2,
};

enum {
	EXAMPLE_CAMERA_MOVE_SPEED	 = 3,
	EXAMPLE_CAMERA_MOUSE_SENSITIVITY = 3,
};

static const float EXAMPLE_CAMERA_NEAR_CLIP = 0.05f;
static const float EXAMPLE_CAMERA_FAR_CLIP  = 100.0f;

typedef struct example_transform_s {
	mat4f_t model;
	mat4f_t view;
	mat4f_t projection;
} example_transform_t;

typedef struct example_camera_s {
	vec3f_t position;
	float yaw;
	float pitch;
	int forward;
	int back;
	int left;
	int right;
	int up;
	int down;
	int cursor_centered;
	u64 frame_time;
} example_camera_t;

typedef struct example_target_s {
	gfx_driver_t *driver;
	gfx_t gfx;
	gfx_buffer_t vb;
	gfx_buffer_t rect_ib;
	gfx_buffer_t cube_ib;
	gfx_buffer_t gui_ub;
	gfx_buffer_t world_ub;
	gfx_shader_t vs;
	gfx_shader_t fs;
	gfx_render_pass_t render_pass;
	gfx_framebuffer_t framebuffer;
	gfx_pipeline_t gui_pipeline;
	gfx_pipeline_t world_pipeline;
	gfx_swapchain_t swapchain;
	gfx_image_t swapchain_images[8];
	gfx_image_t *frame_image;
	surface_t surface;
	window_t window;
	example_transform_t transform;
	example_camera_t camera;
	u32 id;
	u16 width;
	u16 height;
	u64 draw_time;
	u64 fps_time;
	u32 fps_frames;
	u32 fps;
	int redraw;
	int open;
	int initialized;
} example_target_t;

typedef struct example_vertex_s {
	float x;
	float y;
	float z;
	float r;
	float g;
	float b;
	float a;
} example_vertex_t;

typedef struct example_state_s {
	example_target_t *targets;
	u32 count;
	u32 open;
	proc_t *proc;
	display_t *display;
	gfx_shader_compiler_t *shader_compiler;
	int failed;
} example_state_t;

static int example_target_set_cursor_centered(example_target_t *target, int centered);

static void example_camera_init(example_camera_t *camera)
{
	if (camera == NULL) {
		return;
	}
	*camera = (example_camera_t){
		.position = {0.0f, 0.0f, 5.0f},
	};
}

static int example_camera_moving(const example_camera_t *camera)
{
	return camera != NULL && (camera->forward || camera->back || camera->left || camera->right || camera->up || camera->down);
}

static void example_camera_basis(const example_camera_t *camera, vec3f_t *forward, vec3f_t *right, vec3f_t *up)
{
	float cy = float_cos(camera->yaw);
	float sy = float_sin(camera->yaw);
	float cp = float_cos(camera->pitch);
	float sp = float_sin(camera->pitch);

	vec3f_t f = vec3f(sy * cp, sp, -cy * cp);
	vec3f_t r = vec3f(cy, 0.0f, sy);
	vec3f_t u = vec3f_cross(r, f);
	if (forward != NULL) {
		*forward = f;
	}
	if (right != NULL) {
		*right = r;
	}
	if (up != NULL) {
		*up = u;
	}
}

static mat4f_t example_camera_view(const example_camera_t *camera)
{
	vec3f_t forward = {0};
	example_camera_basis(camera, &forward, NULL, NULL);
	return mat4f_look_to(camera->position, forward, vec3f(0.0f, 1.0f, 0.0f));
}

static mat4f_t example_projection(u16 width, u16 height)
{
	float aspect = height != 0 ? (float)width / (float)height : 1.0f;
	float top    = EXAMPLE_CAMERA_NEAR_CLIP;
	float right  = aspect * top;
	return mat4f_frustum(-right, right, -top, top, EXAMPLE_CAMERA_NEAR_CLIP, EXAMPLE_CAMERA_FAR_CLIP);
}

static example_transform_t example_gui_transform(void)
{
	return (example_transform_t){
		.model	    = mat4f_identity(),
		.view	    = mat4f_identity(),
		.projection = mat4f_identity(),
	};
}

static int update_target_camera(example_target_t *target, u64 now)
{
	if (target == NULL) {
		return 1;
	}
	if (target->camera.frame_time == 0) {
		target->camera.frame_time = now;
	}
	u64 elapsed		  = now - target->camera.frame_time;
	target->camera.frame_time = now;
	if (elapsed > 100) {
		elapsed = 100;
	}

	if (elapsed > 0 && example_camera_moving(&target->camera)) {
		float dt	= (float)elapsed * 0.001f;
		float distance	= (float)EXAMPLE_CAMERA_MOVE_SPEED * dt;
		vec3f_t forward = {0};
		vec3f_t right	= {0};
		example_camera_basis(&target->camera, &forward, &right, NULL);
		vec3f_t move = {0};
		if (target->camera.forward) {
			move = vec3f_add(move, forward);
		}
		if (target->camera.back) {
			move = vec3f_sub(move, forward);
		}
		if (target->camera.right) {
			move = vec3f_add(move, right);
		}
		if (target->camera.left) {
			move = vec3f_sub(move, right);
		}
		if (target->camera.up) {
			move.y += 1.0f;
		}
		if (target->camera.down) {
			move.y -= 1.0f;
		}
		float len2 = vec3f_len2(move);
		if (len2 > 0.0f) {
			float scale		= distance / float_sqrt(len2);
			target->camera.position = vec3f_add(target->camera.position, vec3f_scale(move, scale));
			target->redraw		= 1;
		}
	}

	target->transform.view	     = example_camera_view(&target->camera);
	target->transform.projection = example_projection(target->width, target->height);
	return 0;
}

static int draw(example_target_t *target, u64 now)
{
	if (target == NULL) {
		return 1;
	}

	if (update_target_camera(target, now)) {
		log_error("csurface_example", "draw", NULL, "failed to update camera transform");
		return 1;
	}

	gfx_frame_t frame	    = {0};
	gfx_swapchain_image_t image = {0};
	if (gfx_swapchain_acquire(&target->swapchain, &image)) {
		log_error("csurface_example", "draw", NULL, "failed to acquire swapchain image");
		return 1;
	}

	gfx_pass_config_t pass_config = {
		.clear	     = {0.1f, 0.2f, 0.3f, 1.0f},
		.clear_depth = 1.0f,
		.viewport    = {0, 0, target->width, target->height},
	};
	if (gfx_framebuffer_pass_begin(&target->framebuffer, &frame, &pass_config)) {
		log_error("csurface_example", "draw", NULL, "failed to begin render pass");
		return 1;
	}
	if (gfx_pipeline_bind(&frame, &target->world_pipeline)) {
		log_error("csurface_example", "draw", NULL, "failed to bind world pipeline");
		gfx_end(&frame);
		return 1;
	}
	if (gfx_buffer_bind(&frame, &target->vb)) {
		log_error("csurface_example", "draw", NULL, "failed to bind vertex buffer");
		gfx_end(&frame);
		return 1;
	}
	const gfx_resource_binding_t world_resources[] = {
		{.binding = 0, .type = GFX_RESOURCE_UNIFORM_BUFFER, .buffer = &target->world_ub},
	};
	if (gfx_buffer_set_data(&target->world_ub, &target->transform, sizeof(target->transform)) ||
	    gfx_bind_resources(&frame, world_resources, (u32)(sizeof(world_resources) / sizeof(world_resources[0]))) ||
	    gfx_buffer_bind(&frame, &target->cube_ib)) {
		log_error("csurface_example", "draw", NULL, "failed to bind cube draw state");
		gfx_end(&frame);
		return 1;
	}
	if (gfx_draw_indexed(&frame, EXAMPLE_CUBE_INDEX_COUNT)) {
		log_error("csurface_example", "draw", NULL, "failed to draw cube");
		gfx_end(&frame);
		return 1;
	}
	if (gfx_pipeline_bind(&frame, &target->gui_pipeline) || gfx_buffer_bind(&frame, &target->vb)) {
		log_error("csurface_example", "draw", NULL, "failed to bind GUI pipeline");
		gfx_end(&frame);
		return 1;
	}
	const gfx_resource_binding_t gui_resources[] = {
		{.binding = 0, .type = GFX_RESOURCE_UNIFORM_BUFFER, .buffer = &target->gui_ub},
	};
	if (gfx_bind_resources(&frame, gui_resources, (u32)(sizeof(gui_resources) / sizeof(gui_resources[0]))) ||
	    gfx_buffer_bind(&frame, &target->rect_ib)) {
		log_error("csurface_example", "draw", NULL, "failed to bind rectangle draw state");
		gfx_end(&frame);
		return 1;
	}
	if (gfx_draw_indexed(&frame, EXAMPLE_RECT_INDEX_COUNT)) {
		log_error("csurface_example", "draw", NULL, "failed to draw rectangle");
		gfx_end(&frame);
		return 1;
	}
	if (gfx_end(&frame)) {
		log_error("csurface_example", "draw", NULL, "failed to end");
		return 1;
	}
	if (gfx_swapchain_present(&target->swapchain, &image)) {
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

static int update_target_title(example_target_t *target)
{
	if (target == NULL || target->driver == NULL) {
		return 1;
	}

	char title[EXAMPLE_TITLE_SIZE];
	if (c_sprintf(title, sizeof(title), 0, "%s - %u FPS", target->driver->name, target->fps) < 0) {
		return 1;
	}

	return window_set_title(&target->window, strv_cstr(title));
}

static int update_target_fps(example_target_t *target, u64 now)
{
	if (target == NULL) {
		return 1;
	}

	target->fps_frames++;
	u64 elapsed = now - target->fps_time;
	if (elapsed < EXAMPLE_FPS_INTERVAL) {
		return 0;
	}

	target->fps	   = (u32)(((u64)target->fps_frames * EXAMPLE_FPS_INTERVAL) / elapsed);
	target->fps_frames = 0;
	target->fps_time   = now;
	return update_target_title(target);
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
	u64 now = c_time();
	for (u32 i = 0; i < count; i++) {
		if (!targets[i].open) {
			continue;
		}
		if (example_camera_moving(&targets[i].camera)) {
			targets[i].redraw = 1;
		}
		if (targets[i].driver->api == GFX_API_SOFTWARE && !targets[i].redraw) {
			continue;
		}
		if (targets[i].driver->api == GFX_API_SOFTWARE && targets[i].draw_time != 0 &&
		    now - targets[i].draw_time < EXAMPLE_SOFTWARE_FRAME_INTERVAL) {
			continue;
		}
		if (draw(&targets[i], now)) {
			log_error("csurface_example", "draw", NULL, "failed to draw with graphics driver: %s", targets[i].driver->name);
			return 1;
		}
		targets[i].draw_time = now;
		targets[i].redraw    = 0;
		if (update_target_fps(&targets[i], now)) {
			log_error("csurface_example",
				  "draw",
				  NULL,
				  "failed to update FPS counter for graphics driver: %s",
				  targets[i].driver->name);
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

	if (target->swapchain.gfx != NULL) {
		int ret = target->framebuffer.gfx != NULL ? gfx_framebuffer_resize(&target->framebuffer, width, height)
							  : gfx_swapchain_resize(&target->swapchain, width, height);
		if (ret) {
			return 1;
		}
		target->width  = width;
		target->height = height;
		return 0;
	}

	surface_native_t native = {0};
	if (surface_native(&target->surface, &native)) {
		return 1;
	}
	gfx_swapchain_config_t swapchain_config = {
		.format		 = GFX_FORMAT_RGBA8,
		.surface	 = native.gfx_surface,
		.width		 = width,
		.height		 = height,
		.present_mode	 = GFX_PRESENT_MODE_IMMEDIATE,
		.images		 = target->swapchain_images,
		.min_image_count = EXAMPLE_SWAPCHAIN_IMAGE_COUNT,
		.image_capacity	 = sizeof(target->swapchain_images) / sizeof(target->swapchain_images[0]),
	};
	if (gfx_swapchain_init(&target->swapchain, &target->gfx, &swapchain_config) == NULL) {
		gfx_swapchain_free(&target->swapchain);
		return 1;
	}
	target->frame_image = &target->swapchain.images[0];
	target->width	    = width;
	target->height	    = height;
	return 0;
}

static void clear_target_graphics(example_target_t *target)
{
	if (target == NULL) {
		return;
	}

	gfx_buffer_free(&target->cube_ib);
	gfx_buffer_free(&target->rect_ib);
	gfx_buffer_free(&target->vb);
	gfx_buffer_free(&target->world_ub);
	gfx_buffer_free(&target->gui_ub);
	gfx_shader_free(&target->vs);
	gfx_shader_free(&target->fs);
	gfx_pipeline_free(&target->world_pipeline);
	gfx_pipeline_free(&target->gui_pipeline);
	gfx_framebuffer_free(&target->framebuffer);
	gfx_swapchain_free(&target->swapchain);
	target->frame_image = NULL;
	gfx_render_pass_free(&target->render_pass);
	surface_free(&target->surface);
	gfx_free(&target->gfx);
	target->driver = NULL;
}

static surface_gfx_config_t target_graphics_config(display_t *display, gfx_driver_t *driver, u32 image_count)
{
	return (surface_gfx_config_t){
		.display = display,
		.driver	 = driver,
		.surface = {.image_count = image_count},
	};
}

static int init_target_graphics(display_t *display, proc_t *proc, gfx_driver_t *driver, example_target_t *target)
{
	if (display == NULL || proc == NULL || driver == NULL || target == NULL) {
		return -1;
	}

	u32 image_count		    = EXAMPLE_SWAPCHAIN_IMAGE_COUNT;
	surface_gfx_config_t config = target_graphics_config(display, driver, image_count);
	if (!surface_gfx_supported(&config)) {
		return 0;
	}

	target->driver = driver;
	if (surface_gfx_init(&target->surface, &target->gfx, &config, proc, ALLOC_STD)) {
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

	example_target_set_cursor_centered(target, 0);
	window_set_raw_motion(&target->window, 0);
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
	window_set_raw_motion(&target->window, 0);
	clear_target_graphics(target);
	window_free(&target->window);
	return -1;
}

static int bind_target_graphics(display_t *display, proc_t *proc, example_target_t *target, window_t *window)
{
	if (display == NULL || proc == NULL || target == NULL || target->driver == NULL || window == NULL) {
		return 1;
	}

	u32 image_count		    = EXAMPLE_SWAPCHAIN_IMAGE_COUNT;
	surface_gfx_config_t config = target_graphics_config(display, target->driver, image_count);
	return surface_gfx_bind(&target->surface, &target->gfx, window, &config, proc, ALLOC_STD);
}

static int init_target_pipeline(example_target_t *target, gfx_shader_compiler_t *compiler)
{
	if (target == NULL || target->driver == NULL) {
		return 1;
	}
	if (target->vs.data != NULL) {
		return 0;
	}
	example_vertex_t vertices[] = {
		{-0.95f, 0.95f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
		{-0.65f, 0.95f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
		{-0.65f, 0.65f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f},
		{-0.95f, 0.65f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f},
		{-0.5f, -0.5f, -0.5f, 0.9f, 0.2f, 0.2f, 1.0f},
		{0.5f, -0.5f, -0.5f, 0.2f, 0.9f, 0.2f, 1.0f},
		{0.5f, 0.5f, -0.5f, 0.2f, 0.2f, 0.9f, 1.0f},
		{-0.5f, 0.5f, -0.5f, 0.9f, 0.9f, 0.2f, 1.0f},
		{-0.5f, -0.5f, 0.5f, 0.9f, 0.2f, 0.9f, 1.0f},
		{0.5f, -0.5f, 0.5f, 0.2f, 0.9f, 0.9f, 1.0f},
		{0.5f, 0.5f, 0.5f, 0.9f, 0.6f, 0.2f, 1.0f},
		{-0.5f, 0.5f, 0.5f, 0.6f, 0.2f, 0.9f, 1.0f},
	};
	gfx_buffer_config_t vertex_buffer_config = {
		.type  = GFX_BUFFER_VERTEX,
		.usage = GFX_BUFFER_USAGE_STATIC,
		.size  = sizeof(vertices),
		.data  = vertices,
	};
	if (gfx_buffer_init(&target->vb, &target->gfx, &vertex_buffer_config) == NULL) {
		log_error("csurface_example",
			  "init",
			  NULL,
			  "failed to initialize geometry vertex buffer for driver: %s",
			  target->driver->name);
		return 1;
	}
	unsigned int rect_indices[]		     = {0, 1, 3, 1, 2, 3};
	gfx_buffer_config_t rect_index_buffer_config = {
		.type  = GFX_BUFFER_INDEX,
		.usage = GFX_BUFFER_USAGE_STATIC,
		.size  = sizeof(rect_indices),
		.data  = rect_indices,
	};
	if (gfx_buffer_init(&target->rect_ib, &target->gfx, &rect_index_buffer_config) == NULL) {
		log_error("csurface_example",
			  "init",
			  NULL,
			  "failed to initialize rectangle index buffer for driver: %s",
			  target->driver->name);
		return 1;
	}
	unsigned int cube_indices[] = {
		4, 7, 6, 4, 6, 5, 4, 8, 11, 4, 11, 7, 5, 6, 10, 5, 10, 9, 7, 11, 10, 7, 10, 6, 4, 5, 9, 4, 9, 8, 8, 9, 10, 8, 10, 11,
	};
	gfx_buffer_config_t cube_index_buffer_config = {
		.type  = GFX_BUFFER_INDEX,
		.usage = GFX_BUFFER_USAGE_STATIC,
		.size  = sizeof(cube_indices),
		.data  = cube_indices,
	};
	if (gfx_buffer_init(&target->cube_ib, &target->gfx, &cube_index_buffer_config) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize cube index buffer for driver: %s", target->driver->name);
		return 1;
	}
	target->transform = (example_transform_t){
		.model	    = mat4f_identity(),
		.view	    = example_camera_view(&target->camera),
		.projection = example_projection(target->width, target->height),
	};
	example_transform_t gui_transform	      = example_gui_transform();
	gfx_buffer_config_t gui_uniform_buffer_config = {
		.type  = GFX_BUFFER_UNIFORM,
		.usage = GFX_BUFFER_USAGE_STATIC,
		.size  = sizeof(gui_transform),
		.data  = &gui_transform,
	};
	if (gfx_buffer_init(&target->gui_ub, &target->gfx, &gui_uniform_buffer_config) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize GUI uniform buffer for driver: %s", target->driver->name);
		return 1;
	}
	gfx_buffer_config_t world_uniform_buffer_config = {
		.type  = GFX_BUFFER_UNIFORM,
		.usage = GFX_BUFFER_USAGE_DYNAMIC,
		.size  = sizeof(target->transform),
		.data  = &target->transform,
	};
	if (gfx_buffer_init(&target->world_ub, &target->gfx, &world_uniform_buffer_config) == NULL) {
		log_error(
			"csurface_example", "init", NULL, "failed to initialize world uniform buffer for driver: %s", target->driver->name);
		return 1;
	}
	const char *shader_src =
		"vs_in 0 VertexIn {\n"
		"\tvec3f position : POSITION;\n"
		"\tvec4f color : COLOR0;\n"
		"}\n"
		"vs_out VertexOut {\n"
		"\tvec4f position : POSITION;\n"
		"\tvec4f color : COLOR0;\n"
		"}\n"
		"fs_in FragmentIn {\n"
		"\tvec4f color : COLOR0;\n"
		"}\n"
		"fs_out FragmentOut {\n"
		"\tvec4f color : COLOR0;\n"
		"}\n"
		"buffer 0 Transform {\n"
		"\tmat4f model;\n"
		"\tmat4f view;\n"
		"\tmat4f projection;\n"
		"}\n"
		"VertexOut vertex(VertexIn input) {\n"
		"\tVertexOut output;\n"
		"\toutput.position = projection * view * model * vec4f(input.position.x, input.position.y, input.position.z, 1.0f);\n"
		"\toutput.color = input.color;\n"
		"\treturn output;\n"
		"}\n"
		"FragmentOut fragment(FragmentIn input) {\n"
		"\tFragmentOut output;\n"
		"\toutput.color = input.color;\n"
		"\treturn output;\n"
		"}\n";

	gfx_shader_config_t vs_config = {
		.compiler = compiler,
		.source	  = strv_cstr(shader_src),
		.stage	  = GFX_SHADER_STAGE_VERTEX,
	};

	if (gfx_shader_init(&target->vs, &target->gfx, &vs_config) == NULL) {
		log_error("csurface_example",
			  "init",
			  NULL,
			  "failed to initialize geometry vertex shader for driver: %s",
			  target->driver->name);
		return 1;
	}

	gfx_shader_config_t fs_config = {
		.compiler = compiler,
		.source	  = strv_cstr(shader_src),
		.stage	  = GFX_SHADER_STAGE_FRAGMENT,
	};
	if (gfx_shader_init(&target->fs, &target->gfx, &fs_config) == NULL) {
		log_error("csurface_example",
			  "init",
			  NULL,
			  "failed to initialize geometry fragment shader for driver: %s",
			  target->driver->name);
		return 1;
	}

	static const gfx_layout_t input_layout[] = {
		{.index = 0, .semantic = "POSITION", .count = 3, .type = GFX_VALUE_FLOAT32},
		{.index = 1, .semantic = "COLOR", .count = 4, .type = GFX_VALUE_FLOAT32},
	};
	if (gfx_render_pass_init(&target->render_pass,
				 &target->gfx,
				 &(gfx_render_pass_config_t){
					 .color_format = target->frame_image->format,
					 .depth_format = GFX_FORMAT_D32_FLOAT,
					 .load	       = GFX_LOAD_CLEAR,
					 .store	       = GFX_STORE_STORE,
					 .depth_load   = GFX_LOAD_CLEAR,
				 }) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize render pass for driver: %s", target->driver->name);
		return 1;
	}
	if (gfx_framebuffer_init(&target->framebuffer, target->frame_image, &target->render_pass) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize framebuffer for driver: %s", target->driver->name);
		return 1;
	}

	gfx_pipeline_config_t pipeline_config = {
		.render_pass	   = &target->render_pass,
		.vs		   = target->vs,
		.fs		   = target->fs,
		.input_layout	   = input_layout,
		.input_layout_size = sizeof(input_layout),
	};
	if (gfx_pipeline_init(&target->gui_pipeline, &target->gfx, &pipeline_config) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize GUI pipeline for driver: %s", target->driver->name);
		return 1;
	}
	pipeline_config.depth = (gfx_depth_state_t){
		.test	 = 1,
		.write	 = 1,
		.compare = GFX_COMPARE_LESS,
	};
	if (gfx_pipeline_init(&target->world_pipeline, &target->gfx, &pipeline_config) == NULL) {
		log_error("csurface_example", "init", NULL, "failed to initialize world pipeline for driver: %s", target->driver->name);
		return 1;
	}
	return 0;
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

	example_target_t next = {
		.width = target->width, .height = target->height, .camera = target->camera, .transform = target->transform};
	next.camera.frame_time = 0;
	int initialized	       = init_target_graphics(state->display, state->proc, driver, &next);
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
	clear_target_graphics(&next);

	gfx_driver_t *old_driver = target->driver;
	clear_target_graphics(target);
	int target_initialized = init_target_graphics(state->display, state->proc, driver, target);
	if (target_initialized <= 0 || bind_target_graphics(state->display, state->proc, target, &target->window) ||
	    set_target_size(target, target->width, target->height) || init_target_pipeline(target, state->shader_compiler)) {
		clear_target_graphics(target);
		if (init_target_graphics(state->display, state->proc, old_driver, target) <= 0 || restore_target_graphics(state, target) ||
		    init_target_pipeline(target, state->shader_compiler)) {
			return -1;
		}
		return 0;
	}

	target->redraw = 1;
	if (update_target_title(target)) {
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

static int example_camera_key(example_camera_t *camera, display_key_t key, int down)
{
	if (camera == NULL) {
		return 0;
	}
	switch (key) {
	case DISPLAY_KEY_W:
		camera->forward = down;
		return 1;
	case DISPLAY_KEY_S:
		camera->back = down;
		return 1;
	case DISPLAY_KEY_A:
		camera->left = down;
		return 1;
	case DISPLAY_KEY_D:
		camera->right = down;
		return 1;
	case DISPLAY_KEY_SPACE:
		camera->up = down;
		return 1;
	case DISPLAY_KEY_LEFT_SHIFT:
	case DISPLAY_KEY_RIGHT_SHIFT:
		camera->down = down;
		return 1;
	default:
		return 0;
	}
}

static void example_camera_clear_input(example_camera_t *camera)
{
	if (camera == NULL) {
		return;
	}
	camera->forward		= 0;
	camera->back		= 0;
	camera->left		= 0;
	camera->right		= 0;
	camera->up		= 0;
	camera->down		= 0;
	camera->cursor_centered = 0;
}

static int example_target_set_cursor_centered(example_target_t *target, int centered)
{
	if (target == NULL || !target->open) {
		return 0;
	}

	int enabled = centered != 0;
	if (target->camera.cursor_centered == enabled) {
		return 0;
	}
	window_cursor_mode_t mode = enabled ? WINDOW_CURSOR_MODE_CENTERED : WINDOW_CURSOR_MODE_NORMAL;
	if (enabled) {
		if (window_set_cursor_mode(&target->window, mode)) {
			return 1;
		}
		if (window_set_cursor_visible(&target->window, 0)) {
			window_set_cursor_mode(&target->window, WINDOW_CURSOR_MODE_NORMAL);
			return 1;
		}
	} else {
		int ret = window_set_cursor_visible(&target->window, 1);
		ret	= window_set_cursor_mode(&target->window, mode) || ret;
		if (ret) {
			return 1;
		}
	}

	target->camera.cursor_centered = enabled;
	return 0;
}

static int example_target_set_raw_motion(example_state_t *state, example_target_t *target)
{
	if (state == NULL || target == NULL || !target->open) {
		return 0;
	}

	for (u32 i = 0; i < state->count; i++) {
		example_target_t *other = &state->targets[i];
		if (other != target && other->open && window_set_raw_motion(&other->window, 0)) {
			return 1;
		}
	}

	return window_set_raw_motion(&target->window, 1);
}

static void example_camera_mouse_move(example_camera_t *camera, s32 dx, s32 dy)
{
	if (camera == NULL) {
		return;
	}
	float sensitivity = (float)EXAMPLE_CAMERA_MOUSE_SENSITIVITY * 0.001f;
	camera->yaw += (float)dx * sensitivity;
	camera->pitch = float_clamp(camera->pitch - (float)dy * sensitivity, -1.5f, 1.5f);
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
			example_target_set_cursor_centered(target, 0);
			target->open = 0;
			state->open--;
		}
		return;
	case DISPLAY_EVENT_KEY_DOWN:
		switch (event->key) {
		case DISPLAY_KEY_ESCAPE:
			if (example_target_set_cursor_centered(target, 0)) {
				log_error("csurface_example",
					  "event",
					  NULL,
					  "failed to disable centered cursor for graphics driver: %s",
					  target->driver->name);
				state->failed = 1;
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
		if (example_camera_key(&target->camera, event->key, 1)) {
			target->redraw = 1;
			return;
		}
		break;
	case DISPLAY_EVENT_KEY_UP:
		if (example_camera_key(&target->camera, event->key, 0)) {
			target->redraw = 1;
			return;
		}
		break;
	case DISPLAY_EVENT_MOUSE_MOVE_RAW:
		if (!target->open) {
			return;
		}
		if (target->camera.cursor_centered) {
			example_camera_mouse_move(&target->camera, event->dx, event->dy);
			target->redraw = 1;
		}
		break;
	case DISPLAY_EVENT_MOUSE_DOWN:
		if (!target->open) {
			return;
		}
		if (example_target_set_raw_motion(state, target)) {
			log_warn("csurface_example",
				 "event",
				 NULL,
				 "failed to enable raw motion for graphics driver: %s",
				 target->driver->name);
		}
		if (example_target_set_cursor_centered(target, 1)) {
			log_error("csurface_example",
				  "event",
				  NULL,
				  "failed to enable centered cursor for graphics driver: %s",
				  target->driver->name);
			state->failed = 1;
			return;
		}
		break;
	case DISPLAY_EVENT_MOUSE_UP:
		break;
	case DISPLAY_EVENT_FOCUS_LOST:
		if (example_target_set_cursor_centered(target, 0)) {
			log_error("csurface_example",
				  "event",
				  NULL,
				  "failed to disable centered cursor for graphics driver: %s",
				  target->driver->name);
			state->failed = 1;
			return;
		}
		example_camera_clear_input(&target->camera);
		target->redraw = 1;
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
			return;
		}
		target->redraw = 1;
		break;
	default:
		break;
	}
}

static int open_target(display_t *display, proc_t *proc, gfx_driver_t *driver, const display_monitor_t *monitor, u32 index,
		       gfx_shader_compiler_t *compiler, example_target_t *target)
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
	target->fps_time = c_time();
	if (update_target_title(target)) {
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
	example_camera_init(&target->camera);
	if (init_target_pipeline(target, compiler)) {
		return fail_target_init(target);
	}
	target->id	    = window_id(&target->window);
	target->redraw	    = 1;
	target->open	    = 1;
	target->initialized = 1;

	return 1;
}

static int has_continuous_targets(example_target_t *targets, u32 count)
{
	for (u32 i = 0; i < count; i++) {
		if (targets[i].open &&
		    (targets[i].driver->api != GFX_API_SOFTWARE || example_camera_moving(&targets[i].camera) || targets[i].redraw)) {
			return 1;
		}
	}

	return 0;
}

static int run_display_driver(display_driver_t *display_driver, fs_t *fs, proc_t *proc, sock_t *sock,
			      gfx_shader_compiler_t *shader_compiler)
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
	if (shader_compiler == NULL) {
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
		int opened = open_target(&display,
					 proc,
					 drivers[i],
					 has_monitor ? &show_monitor : NULL,
					 target_count,
					 shader_compiler,
					 &targets[target_count]);
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
		.targets	 = targets,
		.count		 = target_count,
		.open		 = target_count,
		.proc		 = proc,
		.display	 = &display,
		.shader_compiler = shader_compiler,
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
		int event_ret =
			has_continuous_targets(targets, target_count) ? display_poll_events(&display) : display_wait_events(&display);
		if (event_ret) {
			log_error("csurface_example",
				  "event",
				  NULL,
				  "failed to process display events from driver: %s",
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
	log_add_callback(log_std_cb, DST_STD(), LOG_INFO, 1, 1);

	fs_t fs						= {0};
	proc_t proc					= {0};
	sock_t sock					= {0};
	gfx_shader_compiler_t shader_compiler		= {0};
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
		if (gfx_shader_compiler_init(&shader_compiler, ALLOC_STD) == NULL) {
			log_error("csurface_example", "init", NULL, "failed to initialize shader compiler");
			ret = 1;
		}
	}
	if (ret == 0) {
		u32 tried = 0;
		for (u32 i = 0; i < driver_count; i++) {
			if (drivers[i] == NULL || drivers[i]->native == NULL) {
				continue;
			}
			if (!display_driver_available(drivers[i], &proc)) {
				continue;
			}
			tried++;
			if (run_display_driver(drivers[i], &fs, &proc, &sock, &shader_compiler)) {
				ret = 1;
				break;
			}
		}
		if (tried == 0) {
			log_error("csurface_example", "init", NULL, "no native display drivers found");
			ret = 1;
		}
	}

	gfx_shader_compiler_free(&shader_compiler);
	sock_free(&sock);
	proc_free(&proc);
	fs_free(&fs);
	return ret;
}

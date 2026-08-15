#include "surface_driver.h"

#include "display_driver.h"
#include "gfx_driver.h"
#include "test.h"

static int t_backend_init_calls;
static int t_backend_free_calls;
static int t_backend_config_window_calls;
static int t_backend_bind_calls;
static int t_backend_unbind_calls;
static int t_backend_native_calls;
static int t_backend_init_ret;
static int t_backend_free_order;
static int t_backend_unbind_ret;
static int t_backend_bind_ret;
static int t_backend_native_ret;
static int t_compatible_ret;
static int t_plan_ret;
static int t_plan_calls;
static int t_gfx_init_calls;
static int t_gfx_free_calls;
static int t_gfx_init_ret;
static int t_gfx_free_order;
static int t_free_sequence;
static gfx_api_t t_compatible_gfx_api;
static display_native_type_t t_compatible_native_type;
static gfx_surface_t *t_native_gfx_surface;
static gfx_config_t t_gfx_init_config;

static int t_backend_init(surface_backend_t *backend, const surface_backend_config_t *config)
{
	(void)config;
	t_backend_init_calls++;
	backend->data = (void *)0x1234;
	return t_backend_init_ret;
}

static int t_backend_free(surface_backend_t *backend)
{
	t_backend_free_calls++;
	t_backend_free_order = ++t_free_sequence;
	backend->data	     = NULL;
	return 0;
}

static int t_backend_config_window(surface_backend_t *backend, window_config_t *config)
{
	(void)backend;
	(void)config;
	t_backend_config_window_calls++;
	return 0;
}

static int t_backend_bind(surface_backend_t *backend, window_t *window)
{
	(void)backend;
	(void)window;
	t_backend_bind_calls++;
	return t_backend_bind_ret;
}

static int t_backend_unbind(surface_backend_t *backend)
{
	(void)backend;
	t_backend_unbind_calls++;
	return t_backend_unbind_ret;
}

static int t_backend_native(surface_backend_t *backend, surface_native_t *native)
{
	(void)backend;
	t_backend_native_calls++;
	*native = (surface_native_t){.handle = 0x1234, .gfx_surface = t_native_gfx_surface};
	return t_backend_native_ret;
}

static int t_compatible(const surface_info_t *info)
{
	return t_compatible_ret && info != NULL && info->gfx_api == t_compatible_gfx_api && info->native_type == t_compatible_native_type;
}

static int t_plan(const surface_info_t *info, surface_plan_t *plan)
{
	(void)info;
	t_plan_calls++;
	plan->gfx.instance_extension_count = 7;
	return t_plan_ret;
}

static surface_driver_t t_surface_driver = {
	.name	       = "test",
	.compatible    = t_compatible,
	.plan	       = t_plan,
	.init	       = t_backend_init,
	.free	       = t_backend_free,
	.config_window = t_backend_config_window,
	.bind	       = t_backend_bind,
	.unbind	       = t_backend_unbind,
	.native	       = t_backend_native,
};
SURFACE_DRIVER(t_surface_driver, &t_surface_driver);

static int t_gfx_init(gfx_t *gfx, const gfx_config_t *config)
{
	t_gfx_init_calls++;
	t_gfx_init_config = *config;
	gfx->data	  = (void *)0x5678;
	return t_gfx_init_ret;
}

static int t_gfx_free(gfx_t *gfx)
{
	t_gfx_free_calls++;
	t_gfx_free_order = ++t_free_sequence;
	gfx->data	 = NULL;
	return 0;
}

static int t_display_native(display_t *display, display_native_t *native)
{
	(void)display;
	*native = (display_native_t){
		.type	 = DISPLAY_NATIVE_WINDOWS,
		.display = (void *)0x1234,
	};
	return 0;
}

static int t_display_native_x11(display_t *display, display_native_t *native)
{
	(void)display;
	*native = (display_native_t){
		.type	 = DISPLAY_NATIVE_X11,
		.display = (void *)0x1234,
	};
	return 0;
}

static void *t_alloc_fail(alloc_t *alloc, size_t size)
{
	(void)alloc;
	(void)size;
	return NULL;
}

static display_driver_t t_display_driver = {
	.name	= "test",
	.native = t_display_native,
};

static display_t t_display = {
	.drv = &t_display_driver,
};

static gfx_driver_t t_gfx_driver = {
	.name = "test",
	.api  = GFX_API_OPENGL,
	.init = t_gfx_init,
	.free = t_gfx_free,
};

static void t_reset(void)
{
	t_backend_init_calls		= 0;
	t_backend_free_calls		= 0;
	t_backend_config_window_calls	= 0;
	t_backend_bind_calls		= 0;
	t_backend_unbind_calls		= 0;
	t_backend_native_calls		= 0;
	t_backend_init_ret		= 0;
	t_backend_free_order		= 0;
	t_backend_unbind_ret		= 0;
	t_backend_bind_ret		= 0;
	t_backend_native_ret		= 0;
	t_compatible_ret		= 1;
	t_plan_ret			= 0;
	t_plan_calls			= 0;
	t_gfx_init_calls		= 0;
	t_gfx_free_calls		= 0;
	t_gfx_init_ret			= 0;
	t_gfx_free_order		= 0;
	t_free_sequence			= 0;
	t_compatible_gfx_api		= GFX_API_OPENGL;
	t_compatible_native_type	= DISPLAY_NATIVE_WINDOWS;
	t_native_gfx_surface		= NULL;
	t_gfx_init_config		= (gfx_config_t){0};
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_BEFORE_BIND;
	t_surface_driver.plan		= t_plan;
	t_surface_driver.init		= t_backend_init;
	t_surface_driver.bind		= t_backend_bind;
	t_surface_driver.native		= t_backend_native;
	t_gfx_driver.init		= t_gfx_init;
	t_gfx_driver.free		= t_gfx_free;
}

static surface_config_t t_config(void)
{
	return (surface_config_t){
		.display = &t_display,
		.driver	 = &t_gfx_driver,
	};
}

static surface_config_t *t_config_ptr(void)
{
	static surface_config_t config;
	config = t_config();
	return &config;
}

TEST(surface_plan_rejects_invalid_arguments)
{
	START;

	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(NULL, &(surface_plan_config_t){.display = &t_display, .gfx_api = GFX_API_OPENGL}), 1);
	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.gfx_api = GFX_API_OPENGL}), 1);

	END;
}

TEST(surface_plan_skips_non_surface_driver)
{
	START;

	t_reset();
	display_t display   = {.drv = &t_display_driver};
	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &display, .gfx_api = 99}), 1);

	END;
}

TEST(surface_plan_returns_driver_plan)
{
	START;

	t_reset();
	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &t_display, .gfx_api = GFX_API_OPENGL}), 0);
	EXPECT_EQ(plan.gfx.instance_extension_count, 7);

	END;
}

TEST(surface_plan_returns_success_without_driver_plan)
{
	START;

	t_reset();
	t_surface_driver.plan		= NULL;
	display_driver_t display_driver = {
		.name	= "test",
		.native = t_display_native_x11,
	};
	display_t display   = {.drv = &display_driver};
	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &display, .gfx_api = GFX_API_OPENGL}), 0);

	END;
}

TEST(surface_supported_rejects_invalid_config)
{
	START;

	EXPECT_EQ(surface_supported(NULL), 0);
	EXPECT_EQ(surface_supported(&(surface_config_t){0}), 0);

	END;
}

TEST(surface_supported_returns_zero_without_driver)
{
	START;

	t_reset();
	gfx_driver_t gfx_driver = t_gfx_driver;
	gfx_driver.api		= 99;
	surface_config_t config = t_config();
	config.driver		= &gfx_driver;

	EXPECT_EQ(surface_supported(&config), 0);

	END;
}

TEST(surface_supported_accepts_missing_plan)
{
	START;

	t_reset();
	t_surface_driver.plan = NULL;

	EXPECT_EQ(surface_supported(&(surface_config_t){.display = &t_display, .driver = &t_gfx_driver}), 1);

	END;
}

TEST(surface_supported_returns_plan_result)
{
	START;

	t_reset();
	surface_config_t config = t_config();

	t_plan_ret = 1;
	EXPECT_EQ(surface_supported(&config), 0);
	t_plan_ret = 0;
	EXPECT_EQ(surface_supported(&config), 1);

	END;
}

TEST(surface_backend_init_rejects_invalid_arguments)
{
	START;

	t_reset();
	surface_backend_t backend = {0};

	EXPECT_NULL(surface_backend_init(NULL,
					 &(surface_backend_config_t){
						 .display = &t_display,
						 .gfx_api = GFX_API_OPENGL,
					 },
					 ALLOC_STD));
	EXPECT_NULL(surface_backend_init(&backend, NULL, ALLOC_STD));
	EXPECT_NULL(surface_backend_init(&backend, &(surface_backend_config_t){0}, ALLOC_STD));

	END;
}

TEST(surface_backend_init_uses_gfx_api_without_gfx)
{
	START;

	t_reset();
	surface_backend_t backend = {0};

	EXPECT_PTR(surface_backend_init(&backend,
					&(surface_backend_config_t){
						.display = &t_display,
						.gfx_api = GFX_API_OPENGL,
					},
					ALLOC_STD),
		   &backend);
	EXPECT_EQ(t_backend_init_calls, 1);
	EXPECT_EQ(backend.config.gfx_api, GFX_API_OPENGL);
	EXPECT_NULL(backend.config.gfx);

	surface_backend_free(&backend);
	END;
}

TEST(surface_backend_free_rejects_invalid_arguments)
{
	START;

	surface_backend_free(NULL);

	END;
}

TEST(surface_backend_unbind_rejects_invalid_arguments)
{
	START;

	EXPECT_EQ(surface_backend_unbind(NULL), 1);
	EXPECT_EQ(surface_backend_unbind(&(surface_backend_t){0}), 1);

	END;
}

TEST(surface_init_rejects_invalid_config)
{
	START;

	proc_t proc	  = {0};
	surface_t surface = {0};

	EXPECT_EQ(surface_init(NULL, &(surface_config_t){0}, &proc, ALLOC_STD), 1);
	EXPECT_EQ(surface_init(&surface, &(surface_config_t){0}, &proc, ALLOC_STD), 1);

	END;
}

TEST(surface_init_returns_backend_alloc_failure)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	proc_t proc			= {0};
	surface_t surface		= {0};

	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, (alloc_t){.alloc = t_alloc_fail}), 1);
	EXPECT_NULL(surface.backend);

	END;
}

TEST(surface_init_returns_error_without_driver)
{
	START;

	t_reset();
	proc_t proc		= {0};
	gfx_driver_t driver	= t_gfx_driver;
	driver.api		= 99;
	surface_config_t config = t_config();
	config.driver		= &driver;
	surface_t surface	= {0};

	EXPECT_EQ(surface_init(&surface, &config, &proc, ALLOC_STD), 1);

	END;
}

TEST(surface_init_returns_plan_failure)
{
	START;

	t_reset();
	t_plan_ret		= 1;
	proc_t proc		= {0};
	surface_config_t config = t_config();
	surface_t surface	= {0};

	EXPECT_EQ(surface_init(&surface, &config, &proc, ALLOC_STD), 1);

	END;
}

TEST(surface_init_after_bind_initializes_backend_only)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	proc_t proc			= {0};
	surface_config_t config		= t_config();
	surface_t surface		= {0};

	EXPECT_EQ(surface_init(&surface, &config, &proc, ALLOC_STD), 0);
	EXPECT_EQ(t_backend_init_calls, 1);
	EXPECT_EQ(t_gfx_init_calls, 0);
	EXPECT_PTR(surface.backend->config.display, &t_display);
	EXPECT_EQ(surface.backend->config.gfx_api, GFX_API_OPENGL);
	EXPECT_NULL(surface.backend->config.gfx);
	EXPECT_NULL(surface_gfx(&surface));

	surface_free(&surface);
	END;
}

TEST(surface_init_returns_gfx_init_failure)
{
	START;

	t_reset();
	t_gfx_init_ret		= 1;
	proc_t proc		= {0};
	surface_config_t config = t_config();
	surface_t surface	= {0};

	EXPECT_EQ(surface_init(&surface, &config, &proc, ALLOC_STD), 1);
	EXPECT_EQ(t_gfx_init_calls, 1);
	EXPECT_NULL(surface.gfx.drv);

	END;
}

TEST(surface_init_frees_gfx_on_backend_failure)
{
	START;

	t_reset();
	t_backend_init_ret	= 1;
	proc_t proc		= {0};
	surface_config_t config = t_config();
	surface_t surface	= {0};

	EXPECT_EQ(surface_init(&surface, &config, &proc, ALLOC_STD), 1);
	EXPECT_EQ(t_gfx_init_calls, 1);
	EXPECT_EQ(t_gfx_free_calls, 1);
	EXPECT_NULL(surface.gfx.drv);

	END;
}

TEST(surface_init_initializes_gfx_before_backend)
{
	START;

	t_reset();
	proc_t proc		= {0};
	surface_config_t config = t_config();
	surface_t surface	= {0};

	EXPECT_EQ(surface_init(&surface, &config, &proc, ALLOC_STD), 0);
	EXPECT_EQ(t_gfx_init_calls, 1);
	EXPECT_EQ(t_backend_init_calls, 1);
	EXPECT_PTR(surface.gfx.drv, &t_gfx_driver);
	EXPECT_PTR(surface.backend->config.gfx, &surface.gfx);
	EXPECT_PTR(surface_gfx(&surface), &surface.gfx);
	EXPECT_NOT_NULL(t_gfx_init_config.plan);

	surface_free(&surface);
	END;
}

TEST(surface_config_window_calls_driver)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	proc_t proc			= {0};
	surface_t surface		= {0};
	window_config_t window_config	= {0};

	EXPECT_EQ(surface_config_window(NULL, &window_config), 1);
	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	EXPECT_EQ(surface_config_window(&surface, NULL), 1);
	EXPECT_EQ(surface_config_window(&surface, &window_config), 0);
	EXPECT_EQ(t_backend_config_window_calls, 1);

	surface_free(&surface);
	END;
}

TEST(surface_bind_rejects_invalid_arguments)
{
	START;

	window_t window = {0};

	EXPECT_EQ(surface_bind(NULL, &window), 1);
	EXPECT_EQ(surface_bind(&(surface_t){0}, NULL), 1);

	END;
}

TEST(surface_bind_returns_bind_failure)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	t_backend_bind_ret		= 1;
	proc_t proc			= {0};
	surface_t surface		= {0};
	window_t window			= {0};

	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	EXPECT_EQ(t_backend_bind_calls, 1);

	surface_free(&surface);
	END;
}

TEST(surface_bind_returns_success_when_gfx_ready)
{
	START;

	t_reset();
	proc_t proc	  = {0};
	surface_t surface = {0};
	window_t window	  = {0};

	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	EXPECT_EQ(surface_bind(&surface, &window), 0);
	EXPECT_EQ(t_backend_bind_calls, 1);
	EXPECT_EQ(t_gfx_init_calls, 1);

	surface_free(&surface);
	END;
}

TEST(surface_bind_unbinds_without_native_surface)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	proc_t proc			= {0};
	surface_t surface		= {0};
	window_t window			= {0};

	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	EXPECT_EQ(t_backend_bind_calls, 1);
	EXPECT_EQ(t_backend_native_calls, 1);
	EXPECT_EQ(t_backend_unbind_calls, 1);

	surface_free(&surface);
	END;
}

TEST(surface_bind_unbinds_on_plan_failure)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	gfx_surface_t gfx_surface	= {0};
	t_native_gfx_surface		= &gfx_surface;
	proc_t proc			= {0};
	surface_t surface		= {0};
	window_t window			= {0};

	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	t_plan_ret = 1;
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	EXPECT_EQ(t_backend_unbind_calls, 1);
	EXPECT_EQ(t_gfx_init_calls, 0);

	surface_free(&surface);
	END;
}

TEST(surface_bind_unbinds_on_gfx_init_failure)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	t_gfx_init_ret			= 1;
	gfx_surface_t gfx_surface	= {0};
	t_native_gfx_surface		= &gfx_surface;
	proc_t proc			= {0};
	surface_t surface		= {0};
	window_t window			= {0};

	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	EXPECT_EQ(surface_bind(&surface, &window), 1);
	EXPECT_EQ(t_gfx_init_calls, 1);
	EXPECT_EQ(t_backend_unbind_calls, 1);
	EXPECT_NULL(surface.gfx.drv);

	surface_free(&surface);
	END;
}

TEST(surface_bind_initializes_gfx_after_bind)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	gfx_surface_t gfx_surface	= {0};
	t_native_gfx_surface		= &gfx_surface;
	proc_t proc			= {0};
	surface_t surface		= {0};
	window_t window			= {0};

	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	EXPECT_EQ(surface_bind(&surface, &window), 0);
	EXPECT_EQ(t_gfx_init_calls, 1);
	EXPECT_PTR(t_gfx_init_config.surface, &gfx_surface);
	EXPECT_PTR(surface.backend->config.gfx, &surface.gfx);

	surface_free(&surface);
	END;
}

TEST(surface_native_calls_driver_and_sets_native)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	proc_t proc			= {0};
	surface_t surface		= {0};
	surface_native_t native		= {0};

	EXPECT_EQ(surface_native(NULL, &native), 1);
	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	EXPECT_EQ(surface_native(&surface, NULL), 1);
	EXPECT_EQ(surface_native(&surface, &native), 0);
	EXPECT_EQ(t_backend_native_calls, 1);
	EXPECT_EQ(native.handle, 0x1234);

	surface_free(&surface);
	END;
}

TEST(surface_native_returns_driver_result)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	t_backend_native_ret		= 1;
	proc_t proc			= {0};
	surface_t surface		= {0};
	surface_native_t native		= {0};

	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	EXPECT_EQ(surface_native(&surface, &native), 1);

	surface_free(&surface);
	END;
}

TEST(surface_unbind_calls_driver)
{
	START;

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	proc_t proc			= {0};
	surface_t surface		= {0};

	EXPECT_EQ(surface_unbind(NULL), 1);
	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	EXPECT_EQ(surface_unbind(&surface), 0);
	EXPECT_EQ(t_backend_unbind_calls, 1);

	surface_free(&surface);
	END;
}

TEST(surface_free_uses_reverse_initialization_order)
{
	START;

	t_reset();
	proc_t proc	  = {0};
	surface_t surface = {0};

	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	surface_free(&surface);
	EXPECT_EQ(t_backend_free_order, 1);
	EXPECT_EQ(t_gfx_free_order, 2);

	t_reset();
	t_surface_driver.gfx_init_order = SURFACE_GFX_INIT_AFTER_BIND;
	gfx_surface_t gfx_surface	= {0};
	t_native_gfx_surface		= &gfx_surface;
	window_t window			= {0};

	EXPECT_EQ(surface_init(&surface, t_config_ptr(), &proc, ALLOC_STD), 0);
	EXPECT_EQ(surface_bind(&surface, &window), 0);
	surface_free(&surface);
	EXPECT_EQ(t_gfx_free_order, 1);
	EXPECT_EQ(t_backend_free_order, 2);

	END;
}

TEST(surface_free_rejects_invalid_arguments)
{
	START;

	surface_free(NULL);
	surface_free(&(surface_t){0});

	END;
}

TEST(surface_test_driver_disable)
{
	START;

	t_compatible_ret = 0;

	END;
}

STEST(surface)
{
	SSTART;

	RUN(surface_plan_rejects_invalid_arguments);
	RUN(surface_plan_skips_non_surface_driver);
	RUN(surface_plan_returns_driver_plan);
	RUN(surface_plan_returns_success_without_driver_plan);
	RUN(surface_supported_rejects_invalid_config);
	RUN(surface_supported_returns_zero_without_driver);
	RUN(surface_supported_accepts_missing_plan);
	RUN(surface_supported_returns_plan_result);
	RUN(surface_backend_init_rejects_invalid_arguments);
	RUN(surface_backend_init_uses_gfx_api_without_gfx);
	RUN(surface_backend_free_rejects_invalid_arguments);
	RUN(surface_backend_unbind_rejects_invalid_arguments);
	RUN(surface_init_rejects_invalid_config);
	RUN(surface_init_returns_backend_alloc_failure);
	RUN(surface_init_returns_error_without_driver);
	RUN(surface_init_returns_plan_failure);
	RUN(surface_init_after_bind_initializes_backend_only);
	RUN(surface_init_returns_gfx_init_failure);
	RUN(surface_init_frees_gfx_on_backend_failure);
	RUN(surface_init_initializes_gfx_before_backend);
	RUN(surface_config_window_calls_driver);
	RUN(surface_bind_rejects_invalid_arguments);
	RUN(surface_bind_returns_bind_failure);
	RUN(surface_bind_returns_success_when_gfx_ready);
	RUN(surface_bind_unbinds_without_native_surface);
	RUN(surface_bind_unbinds_on_plan_failure);
	RUN(surface_bind_unbinds_on_gfx_init_failure);
	RUN(surface_bind_initializes_gfx_after_bind);
	RUN(surface_native_calls_driver_and_sets_native);
	RUN(surface_native_returns_driver_result);
	RUN(surface_unbind_calls_driver);
	RUN(surface_free_uses_reverse_initialization_order);
	RUN(surface_free_rejects_invalid_arguments);
	RUN(surface_test_driver_disable);

	SEND;
}

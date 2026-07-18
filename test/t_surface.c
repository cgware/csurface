#include "surface_driver.h"

#include "display_driver.h"
#include "gfx_driver.h"
#include "test.h"

static int t_surface_init_calls;
static int t_surface_free_calls;
static int t_surface_config_window_calls;
static int t_surface_bind_calls;
static int t_surface_unbind_calls;
static int t_surface_native_calls;
static int t_surface_init_ret;
static int t_surface_free_ret;
static int t_surface_config_window_ret;
static int t_surface_bind_ret;
static int t_surface_unbind_ret;
static int t_surface_native_ret;
static int t_surface_compatible_ret;
static int t_surface_plan_ret;
static int t_surface_plan_calls;

static int t_surface_init(surface_t *srf, const surface_config_t *config)
{
	(void)config;
	t_surface_init_calls++;
	srf->data = (void *)0x1234;
	return t_surface_init_ret;
}

static int t_surface_free(surface_t *srf)
{
	t_surface_free_calls++;
	srf->data = NULL;
	return t_surface_free_ret;
}

static int t_surface_config_window(surface_t *srf, window_config_t *config)
{
	(void)srf;
	(void)config;
	t_surface_config_window_calls++;
	return t_surface_config_window_ret;
}

static int t_surface_bind(surface_t *srf, window_t *window)
{
	(void)srf;
	(void)window;
	t_surface_bind_calls++;
	return t_surface_bind_ret;
}

static int t_surface_unbind(surface_t *srf)
{
	(void)srf;
	t_surface_unbind_calls++;
	return t_surface_unbind_ret;
}

static int t_surface_native(surface_t *srf, surface_native_t *native)
{
	(void)srf;
	t_surface_native_calls++;
	*native = (surface_native_t){.handle = 0x1234};
	return t_surface_native_ret;
}

static int t_surface_compatible(const surface_info_t *info)
{
	(void)info;
	return t_surface_compatible_ret;
}

static int t_surface_plan(const surface_info_t *info, surface_plan_t *plan)
{
	(void)info;
	t_surface_plan_calls++;
	plan->gfx.instance_extension_count = 7;
	return t_surface_plan_ret;
}

static surface_driver_t t_surface_driver = {
	.name	       = "test",
	.compatible    = t_surface_compatible,
	.plan	       = t_surface_plan,
	.init	       = t_surface_init,
	.free	       = t_surface_free,
	.config_window = t_surface_config_window,
	.bind	       = t_surface_bind,
	.unbind	       = t_surface_unbind,
	.native	       = t_surface_native,
};

static int t_surface_display_native(display_t *display, display_native_t *native)
{
	(void)display;
	*native = (display_native_t){
		.type	 = DISPLAY_NATIVE_WINDOWS,
		.display = (void *)0x1234,
	};
	return 0;
}

static int t_surface_display_native_x11(display_t *display, display_native_t *native)
{
	(void)display;
	*native = (display_native_t){
		.type	 = DISPLAY_NATIVE_X11,
		.display = (void *)0x1234,
	};
	return 0;
}

static display_driver_t t_surface_display_driver = {
	.name	= "test",
	.native = t_surface_display_native,
};

static gfx_driver_t t_surface_gfx_driver = {
	.name = "test",
	.api  = GFX_API_OPENGL,
};

static display_t t_surface_display = {
	.drv = &t_surface_display_driver,
};
static gfx_t t_surface_gfx = {
	.drv = &t_surface_gfx_driver,
};
static surface_config_t t_surface_config = {
	.display = &t_surface_display,
	.gfx	 = &t_surface_gfx,
	.alloc	 = {.alloc = alloc_alloc_std, .realloc = alloc_realloc_std, .free = alloc_free_std},
};

static void t_surface_reset(void)
{
	t_surface_init_calls	      = 0;
	t_surface_free_calls	      = 0;
	t_surface_config_window_calls = 0;
	t_surface_bind_calls	      = 0;
	t_surface_unbind_calls	      = 0;
	t_surface_native_calls	      = 0;
	t_surface_init_ret	      = 0;
	t_surface_free_ret	      = 0;
	t_surface_config_window_ret   = 0;
	t_surface_bind_ret	      = 0;
	t_surface_unbind_ret	      = 0;
	t_surface_native_ret	      = 0;
	t_surface_compatible_ret      = 1;
	t_surface_plan_ret	      = 0;
	t_surface_plan_calls	      = 0;
}

TEST(surface_init_null_surface)
{
	START;

	EXPECT_NULL(surface_init(NULL, &t_surface_config));

	END;
}

TEST(surface_init_null_driver)
{
	START;

	surface_t srf = {0};

	EXPECT_NULL(surface_init_driver(&srf, NULL, &t_surface_config));

	END;
}

TEST(surface_init_null_config)
{
	START;

	surface_t srf = {0};

	EXPECT_NULL(surface_init(&srf, NULL));

	END;
}

TEST(surface_plan_null_plan)
{
	START;

	EXPECT_EQ(surface_plan(NULL, &(surface_plan_config_t){.display = &t_surface_display, .gfx_api = GFX_API_OPENGL}), 1);

	END;
}

TEST(surface_plan_null_display)
{
	START;

	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.gfx_api = GFX_API_OPENGL}), 1);

	END;
}

TEST(surface_plan_skips_non_surface_driver)
{
	START;

	t_surface_reset();
	display_t display   = {.drv = &t_surface_display_driver};
	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &display, .gfx_api = 99}), 1);

	END;
}

TEST(surface_plan_returns_driver_plan)
{
	START;

	t_surface_reset();
	surface_plan_t plan = {0};

	EXPECT_EQ(t_surface_driver.plan(&(surface_info_t){.gfx_api = GFX_API_OPENGL, .native_type = DISPLAY_NATIVE_WINDOWS}, &plan), 0);
	EXPECT_EQ(plan.gfx.instance_extension_count, 7);

	END;
}

TEST(surface_plan_returns_success_without_driver_plan)
{
	START;

	display_driver_t display_driver = {
		.name	= "test",
		.native = t_surface_display_native_x11,
	};
	display_t display   = {.drv = &display_driver};
	surface_plan_t plan = {0};

	EXPECT_EQ(surface_plan(&plan, &(surface_plan_config_t){.display = &display, .gfx_api = GFX_API_OPENGL}), 0);

	END;
}

TEST(surface_init_calls_driver)
{
	START;

	t_surface_reset();
	surface_t srf = {0};

	EXPECT_PTR(surface_init_driver(&srf, &t_surface_driver, &t_surface_config), &srf);
	EXPECT_EQ(t_surface_init_calls, 1);

	END;
}

TEST(surface_init_driver_failure_clears_fields)
{
	START;

	t_surface_reset();
	t_surface_init_ret = 1;
	surface_t srf	   = {0};

	EXPECT_NULL(surface_init_driver(&srf, &t_surface_driver, &t_surface_config));
	EXPECT_NULL(srf.drv);
	EXPECT_NULL(srf.data);

	END;
}

TEST(surface_init_gfx_api_failure)
{
	START;

	surface_t srf		= {0};
	gfx_t gfx		= {0};
	surface_config_t config = t_surface_config;
	config.gfx		= &gfx;

	EXPECT_NULL(surface_init(&srf, &config));

	END;
}

TEST(surface_free_null_surface)
{
	START;

	surface_free(NULL);

	END;
}

TEST(surface_free_without_driver)
{
	START;

	surface_t srf = {0};

	surface_free(&srf);

	END;
}

TEST(surface_config_window_null_surface)
{
	START;

	window_config_t config = {0};

	EXPECT_EQ(surface_config_window(NULL, &config), 1);

	END;
}

TEST(surface_bind_null_surface)
{
	START;

	window_t window = {0};

	EXPECT_EQ(surface_bind(NULL, &window), 1);

	END;
}

TEST(surface_unbind_null_surface)
{
	START;

	EXPECT_EQ(surface_unbind(NULL), 1);

	END;
}

TEST(surface_config_window_calls_driver)
{
	START;

	t_surface_reset();
	surface_t srf	       = {.drv = &t_surface_driver};
	window_config_t config = {0};

	EXPECT_EQ(surface_config_window(&srf, &config), 0);
	EXPECT_EQ(t_surface_config_window_calls, 1);

	END;
}

TEST(surface_bind_calls_driver)
{
	START;

	t_surface_reset();
	surface_t srf	= {.drv = &t_surface_driver};
	window_t window = {0};

	EXPECT_EQ(surface_bind(&srf, &window), 0);
	EXPECT_EQ(t_surface_bind_calls, 1);

	END;
}

TEST(surface_unbind_calls_driver)
{
	START;

	t_surface_reset();
	surface_t srf = {.drv = &t_surface_driver};

	EXPECT_EQ(surface_unbind(&srf), 0);
	EXPECT_EQ(t_surface_unbind_calls, 1);

	END;
}

TEST(surface_init_no_compatible_driver)
{
	START;

	surface_t srf		= {0};
	surface_config_t config = t_surface_config;
	gfx_driver_t drv	= t_surface_gfx_driver;
	drv.api			= -1;
	t_surface_gfx.drv	= &drv;

	EXPECT_NULL(surface_init(&srf, &config));

	t_surface_gfx.drv = &t_surface_gfx_driver;
	END;
}

TEST(surface_config_window_null_config)
{
	START;

	surface_t srf = {.drv = &t_surface_driver};

	EXPECT_EQ(surface_config_window(&srf, NULL), 1);

	END;
}

TEST(surface_bind_null_window)
{
	START;

	surface_t srf = {.drv = &t_surface_driver};

	EXPECT_EQ(surface_bind(&srf, NULL), 1);

	END;
}

TEST(surface_native_null_surface)
{
	START;

	surface_native_t native = {0};

	EXPECT_EQ(surface_native(NULL, &native), 1);

	END;
}

TEST(surface_native_calls_driver)
{
	START;

	t_surface_reset();
	surface_t srf		= {.drv = &t_surface_driver};
	surface_native_t native = {0};

	surface_native(&srf, &native);

	EXPECT_EQ(t_surface_native_calls, 1);

	END;
}

TEST(surface_native_sets_native)
{
	START;

	t_surface_reset();
	surface_t srf		= {.drv = &t_surface_driver};
	surface_native_t native = {0};

	surface_native(&srf, &native);

	EXPECT_EQ(native.handle, 0x1234);

	END;
}

TEST(surface_native_returns_driver_result)
{
	START;

	t_surface_reset();
	t_surface_native_ret	= 1;
	surface_t srf		= {.drv = &t_surface_driver};
	surface_native_t native = {0};

	EXPECT_EQ(surface_native(&srf, &native), 1);

	END;
}

TEST(surface_native_null_native)
{
	START;

	surface_t srf = {.drv = &t_surface_driver};

	EXPECT_EQ(surface_native(&srf, NULL), 1);

	END;
}

STEST(surface)
{
	SSTART;

	RUN(surface_init_null_surface);
	RUN(surface_init_null_driver);
	RUN(surface_init_null_config);
	RUN(surface_plan_null_plan);
	RUN(surface_plan_null_display);
	RUN(surface_plan_skips_non_surface_driver);
	RUN(surface_plan_returns_driver_plan);
	RUN(surface_plan_returns_success_without_driver_plan);
	RUN(surface_init_calls_driver);
	RUN(surface_init_driver_failure_clears_fields);
	RUN(surface_init_gfx_api_failure);
	RUN(surface_init_no_compatible_driver);
	RUN(surface_free_null_surface);
	RUN(surface_free_without_driver);
	RUN(surface_config_window_null_surface);
	RUN(surface_config_window_null_config);
	RUN(surface_config_window_calls_driver);
	RUN(surface_bind_null_surface);
	RUN(surface_bind_null_window);
	RUN(surface_bind_calls_driver);
	RUN(surface_unbind_null_surface);
	RUN(surface_unbind_calls_driver);
	RUN(surface_native_null_surface);
	RUN(surface_native_calls_driver);
	RUN(surface_native_sets_native);
	RUN(surface_native_returns_driver_result);
	RUN(surface_native_null_native);

	SEND;
}

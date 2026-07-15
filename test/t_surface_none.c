#include "test.h"

#include "gfx_driver.h"
#include "surface_driver.h"

static surface_driver_t *t_surface_none_driver(void)
{
	for (driver_t *i = DRIVER_START; i < DRIVER_END; i++) {
		if (i->type == SURFACE_DRIVER_TYPE) {
			surface_driver_t *drv = i->data;
			if (strv_eq(strv_cstr(drv->name), STRV("none"))) {
				return drv;
			}
		}
	}

	return NULL;
}

static display_t t_surface_none_display;
static gfx_driver_t t_surface_none_gfx_driver = {
	.name = "test",
	.api  = GFX_API_NONE,
};
static gfx_t t_surface_none_gfx = {
	.drv = &t_surface_none_gfx_driver,
};
static surface_config_t t_surface_none_config = {
	.display = &t_surface_none_display,
	.gfx	 = &t_surface_none_gfx,
	.alloc	 = {.alloc = alloc_alloc_std, .realloc = alloc_realloc_std, .free = alloc_free_std},
};

TEST(surface_none_driver_is_registered)
{
	START;

	EXPECT_NE(t_surface_none_driver(), NULL);

	END;
}

TEST(surface_none_init_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_none_driver();
	EXPECT_NE(drv, NULL);

	EXPECT_EQ(drv->init(NULL, &t_surface_none_config), 1);

	END;
}

TEST(surface_none_init_success)
{
	START;

	surface_t surface = {0};

	EXPECT_EQ(surface_init(&surface, &t_surface_none_config), &surface);
	EXPECT_EQ(surface.drv, t_surface_none_driver());

	surface_free(&surface);
	END;
}

TEST(surface_none_free_null_surface)
{
	START;

	surface_driver_t *drv = t_surface_none_driver();
	EXPECT_NE(drv, NULL);

	EXPECT_EQ(drv->free(NULL), 1);

	END;
}

STEST(surface_none)
{
	SSTART;

	RUN(surface_none_driver_is_registered);
	RUN(surface_none_init_null_surface);
	RUN(surface_none_init_success);
	RUN(surface_none_free_null_surface);

	SEND;
}

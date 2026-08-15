#include "surface_platform.h"

#include "test.h"

TEST(surface_platform_free_null_surface)
{
	START;

	surface_platform_free(NULL, sizeof(int));

	END;
}

TEST(surface_platform_free_null_data)
{
	START;

	surface_backend_t surface = {0};

	surface_platform_free(&surface, sizeof(int));
	EXPECT_NULL(surface.data);

	END;
}

STEST(surface_platform)
{
	SSTART;

	RUN(surface_platform_free_null_surface);
	RUN(surface_platform_free_null_data);

	SEND;
}

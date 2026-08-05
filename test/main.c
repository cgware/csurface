#include "log.h"
#include "mem.h"
#include "test.h"

STEST(surface);
STEST(surface_d3d11);
STEST(surface_none);
STEST(surface_glx);
STEST(surface_swx);
STEST(surface_wsw);
STEST(surface_wgl);
STEST(surface_vk_wsi);

TEST(csurface)
{
	SSTART;
	RUN(surface);
	RUN(surface_d3d11);
	RUN(surface_none);
	RUN(surface_glx);
	RUN(surface_swx);
	RUN(surface_wsw);
	RUN(surface_wgl);
	RUN(surface_vk_wsi);
	SEND;
}

int main(int argc, char **argv)
{
	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_WARN, 1, 1);

	if (t_init(argc, argv)) {
		return 0;
	}

	t_run(test_csurface, 1);

	int ret = t_finish();

	mem_print(DST_STD());

	if (mem_check()) {
		ret = 1;
	}

	return ret;
}

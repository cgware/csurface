#include "log.h"
#include "mem.h"
#include "test.h"

STEST(surface);
STEST(surface_none);
STEST(surface_glx);
STEST(surface_vk_wsi);

TEST(curface)
{
	SSTART;
	RUN(surface);
	RUN(surface_none);
	RUN(surface_glx);
	RUN(surface_vk_wsi);
	SEND;
}

int main(void)
{
	c_print_init();

	log_t log = {0};
	log_set(&log);
	log_add_callback(log_std_cb, DST_STD(), LOG_WARN, 1, 1);

	t_init();

	t_run(test_curface, 1);

	int ret = t_finish();

	mem_print(DST_STD());

	if (mem_check()) {
		ret = 1;
	}

	return ret;
}

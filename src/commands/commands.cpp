#include "threads.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(commands, LOG_LEVEL_DBG);

K_THREAD_DEFINE(commands_thread_id, ThreadConfig::DefaultStackSize, commands_entry, NULL, NULL,
		NULL, ThreadConfig::CommandsPriority, 0, -1);

void commands_entry(void *p1, void *p2, void *p3)
{
	LOG_INF("Commands Thread Started");

	while (1) {
		LOG_INF("Commands: polling for uplink...");
		k_sleep(K_MSEC(4000));
	}
}

#include "threads.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(comms, LOG_LEVEL_DBG);

K_THREAD_DEFINE(comms_thread_id, ThreadConfig::DefaultStackSize, comms_entry, NULL, NULL, NULL,
		ThreadConfig::CommsPriority, 0, -1);

void comms_entry(void *p1, void *p2, void *p3)
{
	LOG_INF("Communications Thread Started");

	while (1) {
		LOG_INF("Comms: checking telemetry...");
		k_sleep(K_MSEC(3000));
	}
}

#include "threads.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "eyestar-s4_api.h"

LOG_MODULE_REGISTER(comms, LOG_LEVEL_DBG);

K_THREAD_DEFINE(comms_thread_id, ThreadConfig::DefaultStackSize, comms_entry, NULL, NULL, NULL,
		ThreadConfig::CommsPriority, 0, -1);

void comms_entry(void *p1, void *p2, void *p3)
{
	LOG_INF("Communications Thread Started");

	const struct device *modem = DEVICE_DT_GET(DT_ALIAS(modem));
	uint8_t rx_buf[205];
	struct eyestar_transfer_result res;

	int ret = eyestar_transfer(modem, NULL, 0, rx_buf, &res);
	LOG_INF("eyestar_transfer returned %d, tx_status=%d", ret, res.tx_status);

	while (1) {
		k_sleep(K_MSEC(5000));
	}
}

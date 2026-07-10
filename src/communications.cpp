#include "threads.hpp"
#include "communications.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "eyestar_s4.h"

LOG_MODULE_REGISTER(comms, LOG_LEVEL_DBG);

/*
 * Binary latch (max count 1) rather than a counter: Comms only needs to know
 * that "there is new work", not how many times DFA nudged it. Defined at file
 * scope (Zephyr kernel objects are section-placed and belong at file scope, not
 * inside a namespace). Whether Comms blocks on this in place of its fixed sleep
 * is a Communications-thread change left for that increment; DFA already gives
 * it here via comms::wake().
 */
K_SEM_DEFINE(comms_wake_sem, 0, 1);

namespace comms
{

void wake(void)
{
	k_sem_give(&comms_wake_sem);
}

} // namespace comms

K_THREAD_DEFINE(comms_thread_id, ThreadConfig::DefaultStackSize, comms_entry, NULL, NULL, NULL,
		ThreadConfig::CommsPriority, 0, -1);

void comms_entry(void *p1, void *p2, void *p3)
{
	LOG_INF("Communications Thread Started");

	const struct device *modem = DEVICE_DT_GET(DT_ALIAS(modem));
	uint8_t rx_buf[205];
	eyestar_transfer_result_t res;

	int ret = eyestar_transfer(modem, NULL, 0, rx_buf, &res);
	LOG_INF("eyestar_transfer returned %d, tx_status=%d", ret, res.tx_status);

	while (1) {
		k_sleep(K_MSEC(5000));
	}
}

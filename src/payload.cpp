#include "threads.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(payload, LOG_LEVEL_DBG);

// ALLOCATE the thread here, using the centralized config. 
// It starts as K_FOREVER so System Health can control it.
K_THREAD_DEFINE(payload_thread_id, ThreadConfig::DefaultStackSize, payload_entry,
                NULL, NULL, NULL, ThreadConfig::PayloadPriority, 0, -1);

void payload_entry(void *p1, void *p2, void *p3) {
    LOG_INF("Payload Thread Started");

    while (1) {
        LOG_INF("Payload doing work...");
        k_sleep(K_MSEC(2000));
    }
}

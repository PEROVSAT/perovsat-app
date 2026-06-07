#include "threads.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sys_health, LOG_LEVEL_DBG);

void system_health_entry(void *p1, void *p2, void *p3) {
    LOG_INF("System Health: booting, starting all threads...");

    k_thread_start(payload_thread_id);
    // k_thread_start(dfa_thread_id);
    // k_thread_start(comms_thread_id);
    // k_thread_start(commands_thread_id);

    while (1) {
        LOG_INF("System Health: nominal");
        k_sleep(K_MSEC(5000));
    }
}

K_THREAD_DEFINE(sys_health_id, ThreadConfig::DefaultStackSize, system_health_entry,
                NULL, NULL, NULL, ThreadConfig::SysHealthPriority, 0, 0);

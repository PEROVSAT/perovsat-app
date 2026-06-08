#include "threads.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dfa, LOG_LEVEL_DBG);

K_THREAD_DEFINE(dfa_thread_id, ThreadConfig::DefaultStackSize, dfa_entry,
                NULL, NULL, NULL, ThreadConfig::DfaPriority, 0, -1);

void dfa_entry(void *p1, void *p2, void *p3) {
    LOG_INF("DFA Thread Started");

    while (1) {
        LOG_INF("DFA: evaluating state...");
        k_sleep(K_MSEC(1000));
    }
}

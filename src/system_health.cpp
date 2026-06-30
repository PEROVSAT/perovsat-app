#include "threads.hpp"
#include "watchdog.hpp"
#include "payload.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sys_health, LOG_LEVEL_DBG);

/* How often System Health drains heartbeats and re-evaluates the roster. */
static constexpr int WatchdogPollMs = 500;

void system_health_entry(void *p1, void *p2, void *p3)
{
	LOG_INF("System Health: booting, starting all threads...");

	/* Starting a thread is its watchdog registration: arm first, then start.
	 * (epoch_ms, max_missed_cycles, startup_grace_ms) */
	health::watchdog.arm(health::MonitoredThread::Payload, 2000, 3, 5000,
			     payload::CriticalErrors);

	k_thread_start(payload_thread_id);

	// health::watchdog.arm(health::MonitoredThread::Dfa, 1000, 3, 2000);
	// k_thread_start(dfa_thread_id);
	// health::watchdog.arm(health::MonitoredThread::Comms, 600000, 2, 10000);
	// k_thread_start(comms_thread_id);
	// health::watchdog.arm(health::MonitoredThread::Commands, 1000, 3, 2000);
	// k_thread_start(commands_thread_id);

	while (1) {
		health::watchdog.poll();
		k_sleep(K_MSEC(WatchdogPollMs));
	}
}

K_THREAD_DEFINE(sys_health_id, ThreadConfig::DefaultStackSize, system_health_entry, NULL, NULL,
		NULL, ThreadConfig::SysHealthPriority, 0, 0);

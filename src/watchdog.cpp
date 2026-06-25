#include "watchdog.hpp"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(watchdog, LOG_LEVEL_INF);

namespace health
{

/*
 * Shared heartbeat queue. K_MSGQ_DEFINE statically allocates the ring buffer at
 * build time, so neither the queue nor its storage ever touches the heap.
 */
K_MSGQ_DEFINE(heartbeat_msgq, sizeof(Heartbeat), HeartbeatQueueDepth, alignof(Heartbeat));

Watchdog watchdog;

void Watchdog::arm(MonitoredThread id, uint32_t epoch_ms, uint32_t max_missed_cycles,
		   uint32_t startup_grace_ms)
{
	const size_t i = static_cast<size_t>(id);
	if (i >= MonitoredThreadCount) {
		return;
	}

	Slot &s = slots_[i];
	s.epoch_ms = epoch_ms;
	s.max_missed_cycles = max_missed_cycles;
	s.startup_grace_ms = startup_grace_ms;
	s.last_seen_ms = k_uptime_get_32();
	s.active = true;
	s.seen_first = false;
	s.faulted = false;
}

void Watchdog::check_in(MonitoredThread id)
{
	Heartbeat hb;
	hb.thread_id = static_cast<uint8_t>(id);
	hb.uptime_ms = k_uptime_get_32();

	/* K_NO_WAIT: the watchdog must never stall the thread it is watching. A
	 * full queue means System Health is behind, which its own miss check will
	 * already surface, so a dropped heartbeat here is harmless. */
	(void)k_msgq_put(&heartbeat_msgq, &hb, K_NO_WAIT);
}

void Watchdog::poll()
{
	Heartbeat hb;
	while (k_msgq_get(&heartbeat_msgq, &hb, K_NO_WAIT) == 0) {
		record(hb);
	}

	evaluate(k_uptime_get_32());
}

bool Watchdog::is_faulted(MonitoredThread id) const
{
	const size_t i = static_cast<size_t>(id);
	if (i >= MonitoredThreadCount) {
		return false;
	}
	return slots_[i].faulted;
}

void Watchdog::record(const Heartbeat &hb)
{
	if (hb.thread_id >= MonitoredThreadCount) {
		return;
	}

	Slot &s = slots_[hb.thread_id];
	if (!s.active) {
		return;
	}

	s.last_seen_ms = hb.uptime_ms;
	s.seen_first = true;
}

void Watchdog::evaluate(uint32_t now)
{
	for (size_t i = 0; i < MonitoredThreadCount; ++i) {
		Slot &s = slots_[i];
		if (!s.active) {
			continue;
		}

		/* Silence tolerated before a fault: max_missed_cycles full epochs.
		 * Until the first heartbeat arrives, allow the extra startup grace
		 * so one-time init cannot trip a false fault. Multiplying here is
		 * equivalent to the design's "missed_cycles >= max_missed_cycles"
		 * but avoids a divide (and any divide-by-zero on an epoch of 0). */
		uint32_t allowance = s.max_missed_cycles * s.epoch_ms;
		if (!s.seen_first) {
			allowance += s.startup_grace_ms;
		}

		/* Wrap-safe: unsigned subtraction stays correct across the 32-bit
		 * uptime rollover (~49.7 days). Always measured against the current
		 * clock, never a future heartbeat that a dead thread will never send. */
		uint32_t elapsed = now - s.last_seen_ms;

		if (allowance != 0 && elapsed >= allowance) {
			if (!s.faulted) {
				s.faulted = true;
				report_fault(static_cast<MonitoredThread>(i));
			}
		} else {
			s.faulted = false;
		}
	}
}

void Watchdog::report_fault(MonitoredThread id)
{
	LOG_ERR("watchdog: thread %u declared not working", static_cast<unsigned>(id));
	/* TODO: escalate to FDIR (restart the thread / enter safe mode) once those
	 * hooks exist. For now the fault is latched and observable via is_faulted(). */
}

} // namespace health

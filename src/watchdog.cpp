#include "watchdog.hpp"

#include "health_fdir.hpp"

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
		   uint32_t startup_grace_ms, ISupervised *handler)
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
	s.latest_errors = 0;
	s.active = true;
	s.seen_first = false;
	/* Start Dead ("not yet proven alive"); the first live poll lifts it to
	 * Nominal, which is an improvement and so never fires a recovery hook. */
	s.status = HealthStatus::Dead;
	handlers_[i] = handler;
}

void Watchdog::check_in(MonitoredThread id, uint32_t errors)
{
	Heartbeat hb;
	hb.thread_id = static_cast<uint8_t>(id);
	hb.uptime_ms = k_uptime_get_32();
	hb.errors = errors;

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

/*
 * Status is cached: evaluate() writes each slot's HealthStatus once per poll and
 * the getters below only read it. That keeps a cross-thread caller from doing a
 * torn read across several raw fields (liveness + error bits) and computing an
 * inconsistent verdict mid-update; it always sees one self-consistent status
 * word. An aligned single-word read is atomic on Cortex-M, so no lock is needed.
 */
HealthStatus Watchdog::status(MonitoredThread id) const
{
	const size_t i = static_cast<size_t>(id);
	if (i >= MonitoredThreadCount || !slots_[i].active) {
		return HealthStatus::Dead;
	}
	return slots_[i].status;
}

HealthStatus Watchdog::status(MonitoredDevice dev) const
{
	const DeviceMap m = device_map(dev);

	/* A device is only as alive as the thread that drives it. */
	if (status(m.owner) == HealthStatus::Dead) {
		return HealthStatus::Dead;
	}

	const size_t i = static_cast<size_t>(m.owner);
	return (slots_[i].latest_errors & m.dev_mask) ? HealthStatus::Partial
						      : HealthStatus::Nominal;
}

bool Watchdog::is_faulted(MonitoredThread id) const
{
	return status(id) == HealthStatus::Dead;
}

Watchdog::DeviceMap Watchdog::device_map(MonitoredDevice dev)
{
	switch (dev) {
	case MonitoredDevice::Imu:
		return {MonitoredThread::Payload,
			payload_err::ImuNotReady | payload_err::ImuReadFail};
	case MonitoredDevice::AmuZps0:
		return {MonitoredThread::Payload, payload_err::AmuSweepFail0};
	case MonitoredDevice::AmuZps1:
		return {MonitoredThread::Payload, payload_err::AmuSweepFail1};
	case MonitoredDevice::Modem:
		return {MonitoredThread::Comms, comms_err::ModemFault};
	default:
		return {MonitoredThread::Count, 0};
	}
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
	s.latest_errors = hb.errors; /* latest-wins */
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
		const bool alive = !(allowance != 0 && elapsed >= allowance);

		HealthStatus new_status;
		if (!alive) {
			new_status = HealthStatus::Dead;
		} else if (s.latest_errors != 0) {
			new_status = HealthStatus::Partial;
		} else {
			new_status = HealthStatus::Nominal;
		}

		const HealthStatus prev = s.status;
		s.status = new_status;

		/* Fire recovery only on a degradation transition (a strictly worse
		 * status: Nominal->Partial, Nominal->Dead, Partial->Dead). Never on
		 * an improvement. HealthStatus is ordered by rising severity, so a
		 * larger enum value is a worse state. */
		if (static_cast<uint8_t>(new_status) > static_cast<uint8_t>(prev)) {
			ISupervised *h = handlers_[i];
			const RecoveryAction action = h ? h->on_fault(new_status, s.latest_errors)
							: RecoveryAction::LogOnly;
			dispatch_recovery(static_cast<MonitoredThread>(i), action);
		}
	}
}

} // namespace health

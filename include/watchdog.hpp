#pragma once

#include <zephyr/kernel.h>

#include <cstddef>
#include <cstdint>

namespace health
{

/*
 * Compile-time roster of every monitored application thread.
 *
 * The enum value doubles as the thread's index into the watchdog's slot array
 * and as the id carried in each heartbeat. Fixing the roster at compile time is
 * what lets the watchdog avoid all dynamic allocation, and it means the
 * monitored set can never drift away from the set that was actually started.
 */
enum class MonitoredThread : uint8_t {
	Payload = 0,
	Dfa,
	Comms,
	Commands,
	Count,
};

constexpr size_t MonitoredThreadCount = static_cast<size_t>(MonitoredThread::Count);

/*
 * Coarse health of a monitored thread or device. Ordered by severity so a
 * plain numeric comparison detects a degradation (a larger value is worse):
 * Nominal < Partial < Dead.
 */
enum class HealthStatus : uint8_t {
	Nominal,
	Partial,
	Dead,
};

/*
 * Per-subsystem error-bit catalogs.
 *
 * Each namespace is a private numbering space, so the same physical bit can
 * mean entirely different things depending on which thread OR-ed it into its
 * heartbeat. The watchdog never interprets these bits generically; only the
 * device_map below (and the owning worker) knows what a given bit means.
 */
namespace payload_err
{
enum : uint32_t {
	ImuNotReady = 1u << 0,
	ImuReadFail = 1u << 1,
	AmuSweepFail0 = 1u << 2,
	AmuSweepFail1 = 1u << 3,
};
} // namespace payload_err

namespace comms_err
{
enum : uint32_t {
	ModemFault = 1u << 0,
};
} // namespace comms_err

/*
 * Monitored physical devices. Unlike threads a device has no heartbeat of its
 * own; its health is derived from its owning thread's liveness plus the error
 * bits that thread reports for it (see device_map in watchdog.cpp).
 */
enum class MonitoredDevice : uint8_t {
	Imu = 0,
	AmuZps0,
	AmuZps1,
	Modem,
	Count,
};

/* Forward declaration: the watchdog only stores ISupervised by pointer. */
class ISupervised;

/* Number of buffered check-ins. Sized for the busiest burst, not the thread count. */
constexpr size_t HeartbeatQueueDepth = 16;

/* One check-in. Trivially copyable so it can travel through a k_msgq by value. */
struct Heartbeat {
	uint8_t thread_id;  /* index into the monitored-thread roster */
	uint32_t uptime_ms; /* k_uptime_get_32() at send time */
	uint32_t errors;    /* error bits OR-accumulated over the thread's loop iteration */
};

/*
 * Per-thread heartbeat watchdog, owned and polled by System Health.
 *
 * Every byte of state is statically sized: the slot array below and the backing
 * message queue (see watchdog.cpp) are both allocated at build time. There is
 * no heap use anywhere in this class.
 */
class Watchdog
{
      public:
	/*
	 * Register a thread on System Health's start path. Starting a thread is
	 * its registration, so the monitored set always matches the started set.
	 *
	 *   epoch_ms          expected maximum interval between check-ins
	 *   max_missed_cycles missed windows tolerated before a fault is declared
	 *   startup_grace_ms  extra time allowed for the very first check-in
	 *   handler           recovery interface invoked on a degradation
	 *                     transition (may be null -> LogOnly)
	 */
	void arm(MonitoredThread id, uint32_t epoch_ms, uint32_t max_missed_cycles,
		 uint32_t startup_grace_ms, ISupervised *handler);

	/*
	 * Called by a worker at the top/bottom of its loop. Never blocks the
	 * caller. `errors` is that iteration's OR-accumulated error bitmask.
	 */
	static void check_in(MonitoredThread id, uint32_t errors);

	/* Called by System Health every tick: drain the queue, then evaluate. */
	void poll();

	/* Cached health of a monitored thread. Inactive/out-of-range -> Dead. */
	HealthStatus status(MonitoredThread id) const;

	/* Health of a device: derived from its owner thread and reported bits. */
	HealthStatus status(MonitoredDevice dev) const;

	/* Thin back-compat wrapper: a thread is "faulted" iff it is Dead. */
	bool is_faulted(MonitoredThread id) const;

      private:
	struct Slot {
		uint32_t epoch_ms = 0;
		uint32_t max_missed_cycles = 0;
		uint32_t startup_grace_ms = 0;
		uint32_t last_seen_ms = 0;
		uint32_t latest_errors = 0; /* most recent heartbeat's error bits (latest-wins) */
		bool active = false;        /* armed and being monitored */
		bool seen_first = false;    /* has checked in at least once */
		HealthStatus status = HealthStatus::Dead; /* cached, written once per poll */
	};

	/* Static resolution of a device to its owning thread and error mask. */
	struct DeviceMap {
		MonitoredThread owner;
		uint32_t dev_mask;
	};
	static DeviceMap device_map(MonitoredDevice dev);

	void record(const Heartbeat &hb);
	void evaluate(uint32_t now);

	Slot slots_[MonitoredThreadCount];
	ISupervised *handlers_[MonitoredThreadCount] = {};
};

/* The single instance, defined in watchdog.cpp and owned by System Health. */
extern Watchdog watchdog;

} // namespace health

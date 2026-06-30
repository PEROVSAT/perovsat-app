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

/* Number of buffered check-ins. Sized for the busiest burst, not the thread count. */
constexpr size_t HeartbeatQueueDepth = 16;

/* One check-in. Trivially copyable so it can travel through a k_msgq by value. */
struct Heartbeat {
	uint8_t thread_id;  /* index into the monitored-thread roster */
	uint32_t uptime_ms; /* k_uptime_get_32() at send time */
	uint32_t errors;    /* thread error count or flags */
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
	 */
	void arm(MonitoredThread id, uint32_t epoch_ms, uint32_t max_missed_cycles,
		 uint32_t startup_grace_ms);

	/* Called by a worker at the top of its loop. Never blocks the caller. */
	static void check_in(MonitoredThread id, uint32_t errors = 0);

	/* Called by System Health every tick: drain the queue, then evaluate. */
	void poll();

	/* True once the named thread has been declared not working. */
	bool is_faulted(MonitoredThread id) const;

      private:
	struct Slot {
		uint32_t epoch_ms = 0;
		uint32_t max_missed_cycles = 0;
		uint32_t startup_grace_ms = 0;
		uint32_t last_seen_ms = 0;
		bool active = false;     /* armed and being monitored */
		bool seen_first = false; /* has checked in at least once */
		bool faulted = false;    /* currently declared not working */
	};

	void record(const Heartbeat &hb);
	void evaluate(uint32_t now);
	void report_fault(MonitoredThread id);

	Slot slots_[MonitoredThreadCount];
};

/* The single instance, defined in watchdog.cpp and owned by System Health. */
extern Watchdog watchdog;

} // namespace health

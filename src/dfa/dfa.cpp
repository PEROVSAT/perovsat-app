#include "threads.hpp"
#include "dfa/tasks.hpp"
#include "global_flags.hpp"
#include "communications.hpp"
#include "watchdog.hpp"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dfa, LOG_LEVEL_DBG);

K_THREAD_DEFINE(dfa_thread_id, ThreadConfig::DefaultStackSize, dfa_entry, NULL, NULL, NULL,
		ThreadConfig::DfaPriority, 0, -1);

/*
 * DFA thread entry -- a thin orchestrator. This function is a direct transcription
 * of the "Data Filtering & Analysis" flowchart: it sequences the tasks and owns
 * the control flow (heartbeat, op_status branch, wake, yield); the dfa:: task
 * functions own the work.
 */
void dfa_entry(void *p1, void *p2, void *p3)
{
	LOG_INF("DFA Thread Started");

	if (dfa::init() != 0) {
		LOG_ERR("DFA init failed; thread exiting");
		return; /* watchdog will observe the silence and act */
	}

	/*
	 * Errors from a pass are reported on the NEXT pass's heartbeat, which the
	 * flowchart places first in the loop ("Send SysHealth Heartbeat"). Seeded
	 * to 0 so the first heartbeat after startup reports a clean slate.
	 */
	uint32_t err = 0;

	while (1) {
		/* 1. Send SysHealth Heartbeat (carries the prior pass's errors). */
		health::Watchdog::check_in(health::MonitoredThread::Dfa, err);
		err = 0;

		/* 2. Read op_status from the Global Flags and branch. */
		const sys::OpStatus mode = sys::op_status();

		switch (mode) {
		case sys::OpStatus::SafeLow:
			/* SAFE/LOW: preserve resources, do no analysis this epoch. */
			LOG_DBG("op_status SAFE/LOW: yielding without work");
			break;

		case sys::OpStatus::Nominal:
		case sys::OpStatus::High:
			/* 3. Fetch -> transform -> sun exposure -> save to LittleFS. */
			err |= dfa::process(mode);
			/* 4. Delete old (raw) data from LittleFS. */
			err |= dfa::reap_old();
			/* 5. Wake Communications only if a new batch reached the outbox;
			 * nudging Comms with nothing to send just wastes its wakeups. */
			if (dfa::produced_output()) {
				comms::wake();
			}
			break;
		}

		/* 6. Yield CPU for the epoch. */
		k_sleep(K_MSEC(ThreadConfig::DfaEpochMs));
	}
}

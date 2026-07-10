#include "payload/PayloadReading.hpp"
#include "payload/hw.hpp"
#include "payload/measurement.hpp"
#include "system_health.hpp"
#include "threads.hpp"
#include "watchdog.hpp"

/**
 * @file payload.cpp
 * @brief Payload thread entrypoint.
 *
 * Orchestrates one periodic sampling cycle: collect measurements into a
 * `PayloadReading` and persist it into the payload outbox.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(payload, LOG_LEVEL_DBG);

K_THREAD_DEFINE(payload_thread_id, ThreadConfig::PayloadStackSize, payload_entry, NULL, NULL, NULL,
		ThreadConfig::PayloadPriority, 0,
		-1); // the -1 means it can't start itself, so syshealth starts it

void payload_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1); // p_ params are zephyr required bs
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("Payload Thread Started");

	static payload::PayloadReading reading;
	uint32_t record_id = 0;

	/*
	 * Errors from a cycle are reported on the NEXT cycle's heartbeat (same
	 * convention as the DFA thread). Seeded to 0 so the first heartbeat after
	 * startup reports a clean slate.
	 */
	uint32_t err = 0;

	while (1) {
		health::Watchdog::check_in(health::MonitoredThread::Payload, err);
		err = 0;

		reading.boot_count = static_cast<uint32_t>(atomic_get(&boot_count));
		reading.record_id = record_id++;

		payload::measure_imu(reading.imu);
		payload::measure_face(payload::face_z, reading.face_z);
		payload::measure_face(payload::face_x, reading.face_x);

		if (reading.imu.status == payload::ReadingStatus::Missing) {
			err |= health::payload_err::ImuNotReady;
		} else if (reading.imu.status == payload::ReadingStatus::Error) {
			err |= health::payload_err::ImuReadFail;
		}

		if (reading.face_z.ps[0].status == payload::ReadingStatus::Error) {
			err |= health::payload_err::AmuSweepFail0;
		}
		if (reading.face_z.ps[1].status == payload::ReadingStatus::Error) {
			err |= health::payload_err::AmuSweepFail1;
		}

		const int ret = reading.write(reading.boot_count, reading.record_id);
		if (ret != 0) {
			LOG_WRN("payload write failed: %d", ret);
		}

		k_sleep(K_MSEC(10000));
	}
}

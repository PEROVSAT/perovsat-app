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

	while (1) {
		health::Watchdog::check_in(health::MonitoredThread::Payload);

		reading.boot_count = static_cast<uint32_t>(atomic_get(&boot_count));
		reading.record_id = record_id++;

		payload::measure_imu(reading.imu);
		payload::measure_face(payload::face_z, reading.face_z);
		payload::measure_face(payload::face_x, reading.face_x);

		const int ret = reading.write(reading.boot_count, reading.record_id);
		if (ret != 0) {
			LOG_WRN("payload write failed: %d", ret);
		}

		k_sleep(K_MSEC(10000));
	}
}

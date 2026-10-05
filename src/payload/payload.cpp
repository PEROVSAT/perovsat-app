#include "payload/PayloadReading.hpp"
#include "payload/hw.hpp"
#include "syshealth/system_health.hpp"
#include "syshealth/watchdog.hpp"
#include "threads.hpp"

/**
 * @file payload.cpp
 * @brief Payload thread: select faces, operate indexed AMUs, store successful data.
 */

#include <amu.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <cstddef>
#include <cstdint>
#include <stdio.h>

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

	// TODO: Move outbox directory initialization here

	payload::init_amus();

	static payload::PayloadReading reading;
	uint32_t record_id = 0;
	uint32_t face_mask = 0;

	struct sensor_value gyro[3];
	struct sensor_value accel[3];
	payload::iv_sweep_t sweeps[payload::NUM_AMUS];

	while (1) {
		health::Watchdog::check_in(health::MonitoredThread::Payload);

		face_mask = 0;
		reading.reset();

		// Attempt to read the IMU
		if (payload::imu_dev == nullptr || !device_is_ready(payload::imu_dev)) {
			// TODO: Erroring
		} else {
			// TODO: Get data from MPU6050 driver and add to reading
		}

		// Identify which faces should be swept in this cycle
		// Since sun sensor or photodiode is undecided, this currently just selects all
		// faces that have an AMU
		for (size_t i = 0; i < payload::NUM_AMUS; ++i) {
			face_mask |= payload::amus[i].face_bit;
		}

		// Power all AMUs that will be swept
		for (size_t i = 0; i < payload::NUM_AMUS; ++i) {
			if ((payload::amus[i].face_bit & face_mask) != 0U) {
				// If it got selected, this would be where we turn on the AMU for
				// sweeping
			}
		}

		// Read sweeps of powered AMUs
		for (size_t i = 0; i < payload::NUM_AMUS; ++i) {
			if ((payload::amus[i].face_bit & face_mask) != 0U) {
				bool ready = false;
				const int64_t ready_deadline = k_uptime_get() + 5000;

				while (k_uptime_get() < ready_deadline) {
					if (device_is_ready(payload::amus[i].dev)) {
						ready = true;
						break;
					}
					k_sleep(K_MSEC(10));
				}
				if (!ready) {
					LOG_WRN("AMU %u not ready", static_cast<unsigned>(i));
					continue;
				}

				amu_t *amu = amu_from_dev(payload::amus[i].dev);

				if (amu == nullptr || amu_trigger_sweep(amu) != 0) {
					LOG_WRN("AMU %u sweep failed", static_cast<unsigned>(i));
					continue;
				}

				amu_get_sweep_meta(amu, &sweeps[i].meta); // TODO: Error handling
				amu_get_sweep_iv(amu, &sweeps[i].iv);

				reading.add_sweep(&(sweeps[i]), i);
			}
		}

		// Turn off all AMUs
		for (size_t i = 0; i < payload::NUM_AMUS; ++i) {
			// TODO: set_amu_power(payload::amus[i], false);
		}

		// Tell PayloadReading to save data
		char path[64];
		const auto boot = static_cast<uint32_t>(atomic_get(&boot_count));
		const int n = snprintf(path, sizeof(path), "/lfs/payload/outbox/%u_%u.raw", boot,
				       record_id++);
		if (n < 0 || static_cast<size_t>(n) >= sizeof(path)) {
			LOG_WRN("payload path failed");
		} else {
			const int ret = reading.save(path);
			if (ret != 0) {
				LOG_WRN("payload write failed: %d", ret);
			}
		}

		k_sleep(K_MSEC(10000));
	}
}

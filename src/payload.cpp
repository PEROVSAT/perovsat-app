#include "threads.hpp"

#include <amu_api.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(payload, LOG_LEVEL_DBG);

K_THREAD_DEFINE(payload_thread_id, ThreadConfig::PayloadStackSize, payload_entry, NULL, NULL, NULL,
		ThreadConfig::PayloadPriority, 0, -1);

void payload_entry(void *p1, void *p2, void *p3)
{
	const struct device *imu = DEVICE_DT_GET(DT_ALIAS(imu));
	const struct device *amu = DEVICE_DT_GET(DT_ALIAS(amu));

	LOG_INF("Payload Thread Started");

	if (!device_is_ready(imu)) {
		LOG_ERR("IMU not ready");
		return;
	}

	if (!device_is_ready(amu)) {
		LOG_ERR("AMU not ready");
		return;
	}

	struct sensor_value accel[3];
	struct sensor_value gyro[3];

	while (1) {
		sensor_sample_fetch(imu);
		sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel);
		sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro);

		LOG_INF("Accel: %d.%06d %d.%06d %d.%06d m/s^2", accel[0].val1, accel[0].val2,
			accel[1].val1, accel[1].val2, accel[2].val1, accel[2].val2);
		LOG_INF("Gyro: %d.%06d %d.%06d %d.%06d rad/s", gyro[0].val1, gyro[0].val2,
			gyro[1].val1, gyro[1].val2, gyro[2].val1, gyro[2].val2);

		struct iv_sweep sweep;
		int ret = amu_do_iv_sweep(amu, &sweep);

		if (ret != 0) {
			LOG_ERR("IV sweep failed: %d", ret);
		} else {
			LOG_INF("IV sweep: tsensor %.1f->%.1f C, time %u->%u ms",
				(double)sweep.tsensor_start, (double)sweep.tsensor_end,
				sweep.time_start, sweep.time_end);
			LOG_INF("IV point 0: V=%.3f V, I=%.3f A", (double)sweep.voltage[0],
				(double)sweep.current[0]);
			LOG_INF("IV point %d: V=%.3f V, I=%.3f A", IV_POINTS - 1,
				(double)sweep.voltage[IV_POINTS - 1],
				(double)sweep.current[IV_POINTS - 1]);
		}

		k_sleep(K_MSEC(2000));
	}
}

#include "threads.hpp"
#include "watchdog.hpp"

#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(payload, LOG_LEVEL_DBG);

K_THREAD_DEFINE(payload_thread_id, ThreadConfig::DefaultStackSize, payload_entry, NULL, NULL, NULL,
		ThreadConfig::PayloadPriority, 0, -1);

void payload_entry(void *p1, void *p2, void *p3)
{
	const struct device *imu = DEVICE_DT_GET(DT_ALIAS(imu));

	LOG_INF("Payload Thread Started");

	if (!device_is_ready(imu)) {
		LOG_ERR("IMU not ready");
		return;
	}

	struct sensor_value accel[3];
	struct sensor_value gyro[3];

	uint32_t err_count = 0;

	while (1) {
		/* Prove forward progress once per loop iteration. */
		health::Watchdog::check_in(health::MonitoredThread::Payload, err_count);

		sensor_sample_fetch(imu);
		sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel);
		sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro);
		LOG_INF("Accel: %d.%06d %d.%06d %d.%06d m/s^2", accel[0].val1, accel[0].val2,
			accel[1].val1, accel[1].val2, accel[2].val1, accel[2].val2);
		LOG_INF("Gyro: %d.%06d %d.%06d %d.%06d rad/s", gyro[0].val1, gyro[0].val2,
			gyro[1].val1, gyro[1].val2, gyro[2].val1, gyro[2].val2);

		double ax = sensor_value_to_double(&accel[0]);
		double ay = sensor_value_to_double(&accel[1]);
		double az = sensor_value_to_double(&accel[2]);
		double accel_mag = pow(ax * ax + ay * ay + az * az, 0.5);
		int accel_int = (int)accel_mag;
		int accel_frac = (int)((accel_mag - accel_int) * 1000000);
		LOG_INF("Acceleration: %d.%06d m/s^2", accel_int, accel_frac);

		double gx = sensor_value_to_double(&gyro[0]);
		double gy = sensor_value_to_double(&gyro[1]);
		double gz = sensor_value_to_double(&gyro[2]);
		double gyro_mag = pow(gx * gx + gy * gy + gz * gz, 0.5);
		int gyro_int = (int)gyro_mag;
		int gyro_frac = (int)((gyro_mag - gyro_int) * 1000000);
		LOG_INF("Angular Velocity: %d.%06d rad/s", gyro_int, gyro_frac);

		k_sleep(K_MSEC(2000));
	}
}

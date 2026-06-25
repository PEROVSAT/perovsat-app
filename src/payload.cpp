#include "threads.hpp"

#include <amu.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <math.h>

LOG_MODULE_REGISTER(payload, LOG_LEVEL_DBG);

K_THREAD_DEFINE(payload_thread_id, ThreadConfig::PayloadStackSize, payload_entry, NULL, NULL, NULL,
		ThreadConfig::PayloadPriority, 0, -1);

void payload_entry(void *p1, void *p2, void *p3)
{
	const struct device *imu = DEVICE_DT_GET(DT_ALIAS(imu));
	static const struct amu_dt_spec cell_z_ps0_spec = AMU_DT_SPEC_GET(DT_NODELABEL(cell_z_ps0));
	static const struct amu_dt_spec cell_z_ps1_spec = AMU_DT_SPEC_GET(DT_NODELABEL(cell_z_ps1));

	LOG_INF("Payload Thread Started");

	if (!device_is_ready(imu)) {
		LOG_ERR("IMU not ready");
		return;
	}

	if (!device_is_ready(cell_z_ps0_spec.dev)) {
		LOG_ERR("AMU for z_ps0 not ready");
		return;
	}

	if (!device_is_ready(cell_z_ps1_spec.dev)) {
		LOG_ERR("AMU for z_ps1 not ready");
		return;
	}

	struct sensor_value accel[3];
	struct sensor_value gyro[3];

	iv_sweep_t z_ps0_sweep;
	iv_sweep_t z_ps1_sweep;

	while (1) {
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
		double accel_mag = sqrt(ax * ax + ay * ay + az * az);
		int accel_int = (int)accel_mag;
		int accel_frac = (int)((accel_mag - accel_int) * 1000000);
		LOG_INF("Acceleration: %d.%06d m/s^2", accel_int, accel_frac);

		double gx = sensor_value_to_double(&gyro[0]);
		double gy = sensor_value_to_double(&gyro[1]);
		double gz = sensor_value_to_double(&gyro[2]);
		double gyro_mag = sqrt(gx * gx + gy * gy + gz * gz);
		int gyro_int = (int)gyro_mag;
		int gyro_frac = (int)((gyro_mag - gyro_int) * 1000000);
		LOG_INF("Angular Velocity: %d.%06d rad/s", gyro_int, gyro_frac);

		int ret = amu_do_iv_sweep(&cell_z_ps0_spec, &z_ps0_sweep);

		if (ret != 0) {
			LOG_ERR("IV sweep failed: %d", ret);
		} else {
			LOG_INF("IV sweep: tsensor %.1f->%.1f C, time %u->%u ms",
				(double)z_ps0_sweep.tsensor_start, (double)z_ps0_sweep.tsensor_end,
				z_ps0_sweep.time_start, z_ps0_sweep.time_end);
			LOG_INF("IV point 0: V=%.3f V, I=%.3f A", (double)z_ps0_sweep.voltage[0],
				(double)z_ps0_sweep.current[0]);
			LOG_INF("IV point %d: V=%.3f V, I=%.3f A", IV_POINTS - 1,
				(double)z_ps0_sweep.voltage[IV_POINTS - 1],
				(double)z_ps0_sweep.current[IV_POINTS - 1]);
		}

		k_sleep(K_MSEC(2000));
	}
}

#include "payload/measurement.hpp"

/**
 * @file measurement.cpp
 * @brief Implementation of payload measurement routines.
 *
 * Contains the logic to read the IMU, sun sensors (stubbed until ss200 is
 * integrated), and perform AMU IV sweeps, including per-face skip behavior.
 */

#include <amu.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>

#include <string.h>

namespace payload
{

static constexpr float kSunAngleSweepThresholdDeg = 0.0f; /* TODO(ss200): tune threshold */

// Compile-time get IMU or set to null
#if DT_HAS_ALIAS(imu) && DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(imu))
static const struct device *const imu_dev = DEVICE_DT_GET(DT_ALIAS(imu));
static constexpr bool imu_present = true;
#else
static const struct device *const imu_dev = nullptr;
static constexpr bool imu_present = false;
#endif

static void measure_cell(const CellSlot &slot, CellReading &out)
{
	memset(&out, 0, sizeof(out));

	if (!slot.present) {
		out.status = ReadingStatus::Disabled;
		return;
	}

	if (slot.dev == NULL || !device_is_ready(slot.dev)) {
		out.status = ReadingStatus::Missing;
		return;
	}

	if (amu_do_iv_sweep(slot.dev, &out.sweep) != 0) {
		out.status = ReadingStatus::Error;
		return;
	}

	out.status = ReadingStatus::Ok;
}

static void measure_sun(const CellSlot &slot, SunReading &out)
{
	memset(&out, 0, sizeof(out));

	if (!slot.present) {
		out.status = ReadingStatus::Disabled;
		return;
	}

	if (slot.dev == NULL || !device_is_ready(slot.dev)) {
		out.status = ReadingStatus::Missing;
		return;
	}

	/* TODO(ss200): sensor_sample_fetch(slot.dev); sensor_channel_get(slot.dev,
	 * SENSOR_CHAN_<custom_angle>, &out.angle_deg); */
	out.status = ReadingStatus::Error;
}

void measure_face(const FacePayload &face, FaceReading &out)
{
	memset(&out, 0, sizeof(out));
	measure_sun(face.sun, out.sun);

	if (out.sun.status == ReadingStatus::Ok &&
	    out.sun.angle_deg <= kSunAngleSweepThresholdDeg) {
		for (int i = 0; i < 2; i++) {
			out.ref[i].status = face.ref[i].present ? ReadingStatus::Skipped
								: ReadingStatus::Disabled;
		}
		for (int i = 0; i < 6; i++) {
			out.ps[i].status = face.ps[i].present ? ReadingStatus::Skipped
							      : ReadingStatus::Disabled;
		}
		return;
	}

	measure_cell(face.ref[0], out.ref[0]);
	measure_cell(face.ref[1], out.ref[1]);
	for (int i = 0; i < 6; i++) {
		measure_cell(face.ps[i], out.ps[i]);
	}
}

void measure_imu(ImuReading &out)
{
	memset(&out, 0, sizeof(out));

	if (!imu_present) {
		out.status = ReadingStatus::Disabled;
		return;
	}

	if (!device_is_ready(imu_dev)) {
		out.status = ReadingStatus::Missing;
		return;
	}

	if (sensor_sample_fetch(imu_dev) != 0) {
		out.status = ReadingStatus::Error;
		return;
	}

	if (sensor_channel_get(imu_dev, SENSOR_CHAN_ACCEL_XYZ, out.accel) != 0 ||
	    sensor_channel_get(imu_dev, SENSOR_CHAN_GYRO_XYZ, out.gyro) != 0) {
		out.status = ReadingStatus::Error;
		return;
	}

	out.status = ReadingStatus::Ok;
}

} // namespace payload

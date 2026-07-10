#pragma once

/**
 * @file payload_types.hpp
 * @brief Data types for one payload sampling cycle.
 *
 * Defines the in-memory structures used to hold measurements (IMU, sun sensors,
 * and AMU cell sweeps) plus compact status encoding helpers used by
 * `PayloadReading`.
 */

#include <amu_lib.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include <cstdint>

namespace payload
{

enum class ReadingStatus : uint8_t {
	Disabled = 0, /* not enabled in this DT build */
	Missing = 1,  /* enabled, but device_is_ready() failed */
	Skipped = 2,  /* enabled and ready, but intentionally skipped this cycle */
	Error = 3,    /* ready, but the sweep/read call failed */
	Ok = 4,       /* ready and produced valid data */
};

struct CellSlot {
	const struct device *dev;
	bool present; /* compile-time DT-enabled flag */
};

struct FacePayload {
	CellSlot sun; /* placeholder until ss200-driver lands */
	CellSlot ref[2];
	CellSlot ps[6];
};

struct CellReading {
	ReadingStatus status;
	iv_sweep_t sweep; /* valid only when status == Ok */
};

struct SunReading {
	ReadingStatus status;
	float angle_deg; /* valid only when status == Ok */
};

struct ImuReading {
	ReadingStatus status;
	struct sensor_value accel[3];
	struct sensor_value gyro[3];
};

struct FaceReading {
	SunReading sun;
	CellReading ref[2];
	CellReading ps[6];
};

/* Packed into PayloadFileHeader::status_mask. */
enum class StatusItem : uint8_t {
	Imu = 0,
	SunZ = 1,
	SunX = 2,
	ZRef0 = 3,
	ZRef1 = 4,
	ZPs0 = 5,
	ZPs1 = 6,
	ZPs2 = 7,
	ZPs3 = 8,
	ZPs4 = 9,
	ZPs5 = 10,
	XRef0 = 11,
	XRef1 = 12,
	XPs0 = 13,
	XPs1 = 14,
	XPs2 = 15,
	XPs3 = 16,
	XPs4 = 17,
	XPs5 = 18,
	Count = 19,
};

struct PayloadFileHeader {
	uint32_t boot_count;
	uint32_t record_id;
	uint32_t timestamp_ms;
	uint64_t status_mask;
} __attribute__((packed));

} // namespace payload

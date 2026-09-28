#pragma once

/**
 * @file PayloadReading.hpp
 * @brief One sampling cycle: IMU validity plus indexed AMU sweep validity/data.
 *
 * AMU identity is simply the DeviceTree payload-index:
 *   payload-index i == amus[i] == sweeps[i] == sweep_mask bit i.
 *
 * The existing raw-file layout is preserved:
 *   bit 0      IMU gyro[3], then accel[3]
 *   bit 1 + i  AMU sweep with payload-index i
 * Payload bodies are written in ascending bit/index order.
 */

#include "payload/payload_types.hpp"

#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>

#include <cstddef>
#include <cstdint>

struct fs_file_t;

namespace payload
{

class PayloadReading
{
      public:
	void reset();

	void add_sweep(const iv_sweep_t *sweep, size_t index);
	void add_imu(const struct sensor_value *gyro, const struct sensor_value *accel);

	/* 0, or a negative errno. */
	int save(const char *path) const;
	int load(const char *path);

	bool get_sweep(size_t index, iv_sweep_t *sweep) const;
	bool get_imu(struct sensor_value *gyro, struct sensor_value *accel) const;

      private:
	static constexpr uint32_t kKnownFileBits =
		BIT(0) | static_cast<uint32_t>(BIT_MASK(NUM_AMUS) << 1);

	bool imu_valid = false;
	uint32_t sweep_mask = 0;
	struct sensor_value gyro[3] = {};
	struct sensor_value accel[3] = {};
	iv_sweep_t sweeps[NUM_AMUS] = {};

	int write_payloads(struct fs_file_t *file) const;
	int read_payloads(struct fs_file_t *file, bool file_has_imu, uint32_t file_sweep_mask);
};

} // namespace payload

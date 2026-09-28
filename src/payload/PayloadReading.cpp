#include "payload/PayloadReading.hpp"

/**
 * @file PayloadReading.cpp
 * @brief Indexed AMU sample mask plus the payloads selected by it.
 */

#include <zephyr/fs/fs.h>
#include <zephyr/sys/util.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

namespace payload
{

static ssize_t write_all(struct fs_file_t *file, const void *data, size_t len)
{
	const uint8_t *cursor = static_cast<const uint8_t *>(data);
	size_t remaining = len;

	while (remaining > 0) {
		const ssize_t written = fs_write(file, cursor, remaining);
		if (written < 0) {
			return written;
		}
		if (written == 0) {
			return -EIO;
		}
		cursor += written;
		remaining -= static_cast<size_t>(written);
	}

	return static_cast<ssize_t>(len);
}

static ssize_t read_all(struct fs_file_t *file, void *data, size_t len)
{
	uint8_t *cursor = static_cast<uint8_t *>(data);
	size_t remaining = len;

	while (remaining > 0) {
		const ssize_t got = fs_read(file, cursor, remaining);
		if (got < 0) {
			return got;
		}
		if (got == 0) {
			return -EIO;
		}
		cursor += got;
		remaining -= static_cast<size_t>(got);
	}

	return static_cast<ssize_t>(len);
}

static int write_exact(struct fs_file_t *file, const void *data, size_t len)
{
	const ssize_t wrote = write_all(file, data, len);
	return (wrote < 0) ? static_cast<int>(wrote) : 0;
}

static int read_exact(struct fs_file_t *file, void *data, size_t len)
{
	const ssize_t got = read_all(file, data, len);
	return (got < 0) ? static_cast<int>(got) : 0;
}

void PayloadReading::reset()
{
	imu_valid = false;
	sweep_mask = 0;
	memset(gyro, 0, sizeof(gyro));
	memset(accel, 0, sizeof(accel));
	memset(sweeps, 0, sizeof(sweeps)); // TODO: May not need memsets if bitmask is correct
}

void PayloadReading::add_sweep(const iv_sweep_t *sweep, size_t index)
{
	if (sweep == nullptr || index >= NUM_AMUS) {
		return;
	}

	sweeps[index] = *sweep;
	sweep_mask |= static_cast<uint32_t>(BIT(index));
}

void PayloadReading::add_imu(const struct sensor_value *gyro_in,
			     const struct sensor_value *accel_in)
{
	if (gyro_in == nullptr || accel_in == nullptr) {
		return;
	}

	memcpy(gyro, gyro_in, sizeof(gyro));
	memcpy(accel, accel_in, sizeof(accel));
	imu_valid = true;
}

bool PayloadReading::get_sweep(size_t index, iv_sweep_t *sweep) const
{
	if (sweep == nullptr || index >= NUM_AMUS ||
	    (sweep_mask & static_cast<uint32_t>(BIT(index))) == 0U) {
		return false;
	}

	*sweep = sweeps[index];
	return true;
}

bool PayloadReading::get_imu(struct sensor_value *gyro_out, struct sensor_value *accel_out) const
{
	if (gyro_out == nullptr || accel_out == nullptr || !imu_valid) {
		return false;
	}

	memcpy(gyro_out, gyro, sizeof(gyro));
	memcpy(accel_out, accel, sizeof(accel));
	return true;
}

int PayloadReading::write_payloads(struct fs_file_t *file) const
{
	if (imu_valid) {
		int ret = write_exact(file, gyro, sizeof(gyro));
		if (ret < 0) {
			return ret;
		}

		ret = write_exact(file, accel, sizeof(accel));
		if (ret < 0) {
			return ret;
		}
	}

	for (size_t i = 0; i < NUM_AMUS; ++i) {
		if ((sweep_mask & static_cast<uint32_t>(BIT(i))) == 0U) {
			continue;
		}

		const int ret = write_exact(file, &sweeps[i], sizeof(sweeps[i]));
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

int PayloadReading::read_payloads(struct fs_file_t *file, bool file_has_imu,
				  uint32_t file_sweep_mask)
{
	if (file_has_imu) {
		int ret = read_exact(file, gyro, sizeof(gyro));
		if (ret < 0) {
			return ret;
		}

		ret = read_exact(file, accel, sizeof(accel));
		if (ret < 0) {
			return ret;
		}
	}

	for (size_t i = 0; i < NUM_AMUS; ++i) {
		if ((file_sweep_mask & static_cast<uint32_t>(BIT(i))) == 0U) {
			continue;
		}

		const int ret = read_exact(file, &sweeps[i], sizeof(sweeps[i]));
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

int PayloadReading::save(const char *path) const
{
	if (path == nullptr) {
		return -EINVAL;
	}

	char tmp_path[160];
	const int tmp_len = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
	if (tmp_len < 0 || static_cast<size_t>(tmp_len) >= sizeof(tmp_path)) {
		return -ENAMETOOLONG;
	}

	struct fs_file_t file;
	fs_file_t_init(&file);

	int ret = fs_open(&file, tmp_path, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
	if (ret < 0) {
		return ret;
	}

	const uint32_t file_mask =
		(imu_valid ? static_cast<uint32_t>(BIT(0)) : 0U) | (sweep_mask << 1);

	ret = write_exact(&file, &file_mask, sizeof(file_mask));
	if (ret < 0) {
		fs_close(&file);
		fs_unlink(tmp_path);
		return ret;
	}

	ret = write_payloads(&file);
	if (ret < 0) {
		fs_close(&file);
		fs_unlink(tmp_path);
		return ret;
	}

	ret = fs_close(&file);
	if (ret < 0) {
		fs_unlink(tmp_path);
		return ret;
	}

	ret = fs_rename(tmp_path, path);
	if (ret < 0) {
		fs_unlink(tmp_path);
		return ret;
	}

	return 0;
}

int PayloadReading::load(const char *path)
{
	reset();

	if (path == nullptr) {
		return -EINVAL;
	}

	struct fs_file_t file;
	fs_file_t_init(&file);

	int ret = fs_open(&file, path, FS_O_READ);
	if (ret < 0) {
		return ret;
	}

	uint32_t file_mask = 0;
	ret = read_exact(&file, &file_mask, sizeof(file_mask));
	if (ret < 0) {
		fs_close(&file);
		return ret;
	}

	if ((file_mask & ~kKnownFileBits) != 0U) {
		fs_close(&file);
		return -EINVAL;
	}

	const bool file_has_imu = (file_mask & static_cast<uint32_t>(BIT(0))) != 0U;
	const uint32_t file_sweep_mask = file_mask >> 1;

	ret = read_payloads(&file, file_has_imu, file_sweep_mask);
	if (ret < 0) {
		fs_close(&file);
		reset();
		return ret;
	}

	ret = fs_close(&file);
	if (ret < 0) {
		reset();
		return ret;
	}

	imu_valid = file_has_imu;
	sweep_mask = file_sweep_mask;
	return 0;
}

} // namespace payload

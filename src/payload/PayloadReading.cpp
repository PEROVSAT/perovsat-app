#include "payload/PayloadReading.hpp"

/**
 * @file PayloadReading.cpp
 * @brief Serialization implementation for payload outbox records.
 *
 * Provides tmp + rename writes and a compact packed status mask describing
 * Disabled/Missing/Skipped/Error/Ok for each item in the record.
 */

#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>

#include <errno.h>
#include <stdio.h>

LOG_MODULE_DECLARE(payload);

namespace payload
{

uint64_t PayloadReading::pack_status(uint64_t mask, uint8_t index, ReadingStatus status)
{
	const uint64_t cleared = mask & ~(0x7ULL << (index * 3));
	return cleared | ((static_cast<uint64_t>(status) & 0x7ULL) << (index * 3));
}

ReadingStatus PayloadReading::get_status(uint64_t mask, uint8_t index)
{
	return static_cast<ReadingStatus>((mask >> (index * 3)) & 0x7ULL);
}

ssize_t PayloadReading::write_all(struct fs_file_t *file, const void *data, size_t len)
{
	const uint8_t *cursor = static_cast<const uint8_t *>(data);
	size_t remaining = len;

	while (remaining > 0) {
		ssize_t written = fs_write(file, cursor, remaining);
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

ssize_t PayloadReading::read_all(struct fs_file_t *file, void *data, size_t len)
{
	uint8_t *cursor = static_cast<uint8_t *>(data);
	size_t remaining = len;

	while (remaining > 0) {
		ssize_t got = fs_read(file, cursor, remaining);
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

uint64_t PayloadReading::status_mask() const
{
	uint64_t mask = 0;

	mask = pack_status(mask, static_cast<uint8_t>(StatusItem::Imu), imu.status);
	mask = pack_status(mask, static_cast<uint8_t>(StatusItem::SunZ), face_z.sun.status);
	mask = pack_status(mask, static_cast<uint8_t>(StatusItem::SunX), face_x.sun.status);

	mask = pack_status(mask, static_cast<uint8_t>(StatusItem::ZRef0), face_z.ref[0].status);
	mask = pack_status(mask, static_cast<uint8_t>(StatusItem::ZRef1), face_z.ref[1].status);
	for (int i = 0; i < 6; i++) {
		mask = pack_status(mask, static_cast<uint8_t>(StatusItem::ZPs0) + i,
				   face_z.ps[i].status);
	}

	mask = pack_status(mask, static_cast<uint8_t>(StatusItem::XRef0), face_x.ref[0].status);
	mask = pack_status(mask, static_cast<uint8_t>(StatusItem::XRef1), face_x.ref[1].status);
	for (int i = 0; i < 6; i++) {
		mask = pack_status(mask, static_cast<uint8_t>(StatusItem::XPs0) + i,
				   face_x.ps[i].status);
	}

	return mask;
}

int PayloadReading::build_payload_path(char *out, size_t out_len, uint32_t boot_count,
				       uint32_t record_id)
{
	if (out == NULL || out_len == 0) {
		return -EINVAL;
	}

	const int n =
		snprintf(out, out_len, "%s/%u_%u.raw", kOutboxBasePath, boot_count, record_id);
	if (n < 0) {
		return -EINVAL;
	}
	if (static_cast<size_t>(n) >= out_len) {
		return -ENOMEM;
	}

	return 0;
}

int PayloadReading::write(uint32_t boot_count, uint32_t record_id) const
{
	char path[64];
	int ret = build_payload_path(path, sizeof(path), boot_count, record_id);
	if (ret < 0) {
		return ret;
	}

	return write_to_path(path, boot_count, record_id);
}

int PayloadReading::write_to_path(const char *path, uint32_t boot_count, uint32_t record_id) const
{
	if (path == NULL) {
		return -EINVAL;
	}

	char tmp_path[96];
	int path_len = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
	if (path_len < 0 || path_len >= static_cast<int>(sizeof(tmp_path))) {
		return -ENOMEM;
	}

	PayloadFileHeader header = {
		.boot_count = boot_count,
		.record_id = record_id,
		.timestamp_ms = timestamp_ms,
		.status_mask = status_mask(),
	};

	struct fs_file_t file;
	fs_file_t_init(&file);

	int ret = fs_open(&file, tmp_path, FS_O_CREATE | FS_O_RDWR);
	if (ret < 0) {
		return ret;
	}

	ret = static_cast<int>(write_all(&file, &header, sizeof(header)));
	if (ret < 0) {
		fs_close(&file);
		fs_unlink(tmp_path);
		return ret;
	}

	ret = static_cast<int>(write_all(&file, &imu, sizeof(imu)));
	if (ret < 0) {
		fs_close(&file);
		fs_unlink(tmp_path);
		return ret;
	}

	ret = static_cast<int>(write_all(&file, &face_z.sun, sizeof(face_z.sun)));
	if (ret < 0) {
		fs_close(&file);
		fs_unlink(tmp_path);
		return ret;
	}

	ret = static_cast<int>(write_all(&file, &face_x.sun, sizeof(face_x.sun)));
	if (ret < 0) {
		fs_close(&file);
		fs_unlink(tmp_path);
		return ret;
	}

	ret = static_cast<int>(write_all(&file, face_z.ref, sizeof(face_z.ref)));
	if (ret < 0) {
		fs_close(&file);
		fs_unlink(tmp_path);
		return ret;
	}

	ret = static_cast<int>(write_all(&file, face_z.ps, sizeof(face_z.ps)));
	if (ret < 0) {
		fs_close(&file);
		fs_unlink(tmp_path);
		return ret;
	}

	ret = static_cast<int>(write_all(&file, face_x.ref, sizeof(face_x.ref)));
	if (ret < 0) {
		fs_close(&file);
		fs_unlink(tmp_path);
		return ret;
	}

	ret = static_cast<int>(write_all(&file, face_x.ps, sizeof(face_x.ps)));
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

int PayloadReading::read(const char *path)
{
	if (path == NULL) {
		return -EINVAL;
	}

	struct fs_file_t file;
	fs_file_t_init(&file);

	int ret = fs_open(&file, path, FS_O_READ);
	if (ret < 0) {
		return ret;
	}

	PayloadFileHeader header = {};
	ret = static_cast<int>(read_all(&file, &header, sizeof(header)));
	if (ret < 0) {
		fs_close(&file);
		return ret;
	}

	boot_count = header.boot_count;
	record_id = header.record_id;
	timestamp_ms = header.timestamp_ms;

	ret = static_cast<int>(read_all(&file, &imu, sizeof(imu)));
	if (ret < 0) {
		fs_close(&file);
		return ret;
	}

	ret = static_cast<int>(read_all(&file, &face_z.sun, sizeof(face_z.sun)));
	if (ret < 0) {
		fs_close(&file);
		return ret;
	}

	ret = static_cast<int>(read_all(&file, &face_x.sun, sizeof(face_x.sun)));
	if (ret < 0) {
		fs_close(&file);
		return ret;
	}

	ret = static_cast<int>(read_all(&file, face_z.ref, sizeof(face_z.ref)));
	if (ret < 0) {
		fs_close(&file);
		return ret;
	}

	ret = static_cast<int>(read_all(&file, face_z.ps, sizeof(face_z.ps)));
	if (ret < 0) {
		fs_close(&file);
		return ret;
	}

	ret = static_cast<int>(read_all(&file, face_x.ref, sizeof(face_x.ref)));
	if (ret < 0) {
		fs_close(&file);
		return ret;
	}

	ret = static_cast<int>(read_all(&file, face_x.ps, sizeof(face_x.ps)));
	if (ret < 0) {
		fs_close(&file);
		return ret;
	}

	ret = fs_close(&file);
	if (ret < 0) {
		return ret;
	}

	/* Enforce status bytes from the header mask to avoid trusting stale struct contents. */
	imu.status = get_status(header.status_mask, static_cast<uint8_t>(StatusItem::Imu));
	face_z.sun.status = get_status(header.status_mask, static_cast<uint8_t>(StatusItem::SunZ));
	face_x.sun.status = get_status(header.status_mask, static_cast<uint8_t>(StatusItem::SunX));

	face_z.ref[0].status =
		get_status(header.status_mask, static_cast<uint8_t>(StatusItem::ZRef0));
	face_z.ref[1].status =
		get_status(header.status_mask, static_cast<uint8_t>(StatusItem::ZRef1));
	for (int i = 0; i < 6; i++) {
		face_z.ps[i].status =
			get_status(header.status_mask, static_cast<uint8_t>(StatusItem::ZPs0) + i);
	}

	face_x.ref[0].status =
		get_status(header.status_mask, static_cast<uint8_t>(StatusItem::XRef0));
	face_x.ref[1].status =
		get_status(header.status_mask, static_cast<uint8_t>(StatusItem::XRef1));
	for (int i = 0; i < 6; i++) {
		face_x.ps[i].status =
			get_status(header.status_mask, static_cast<uint8_t>(StatusItem::XPs0) + i);
	}

	return 0;
}

} // namespace payload

#pragma once

/**
 * @file PayloadReading.hpp
 * @brief Payload outbox record container + serialization API.
 *
 * `payload::PayloadReading` holds the complete set of data produced by one
 * payload thread cycle and can write it to the filesystem outbox in a
 * reboot-safe way (tmp + rename).
 */

#include "payload/payload_types.hpp"

#include <zephyr/fs/fs.h>

#include <cstddef>
#include <cstdint>

namespace payload
{

class PayloadReading
{
      public:
	static constexpr const char *kOutboxBasePath = "/lfs/payload/outbox";

	uint32_t boot_count = 0;
	uint32_t record_id = 0;
	uint32_t timestamp_ms = 0;
	ImuReading imu = {};
	FaceReading face_z = {};
	FaceReading face_x = {};

	/* Serialize to outbox using @p boot_count and @p record_id for the filename. */
	int write(uint32_t boot_count, uint32_t record_id) const;

	/* Deserialize from @p path into this object. */
	int read(const char *path);

	uint64_t status_mask() const;

      private:
	static int build_payload_path(char *out, size_t out_len, uint32_t boot_count,
				      uint32_t record_id);

	int write_to_path(const char *path, uint32_t boot_count, uint32_t record_id) const;

	static uint64_t pack_status(uint64_t mask, uint8_t index, ReadingStatus status);
	static ReadingStatus get_status(uint64_t mask, uint8_t index);

	static ssize_t write_all(struct fs_file_t *file, const void *data, size_t len);
	static ssize_t read_all(struct fs_file_t *file, void *data, size_t len);
};

} // namespace payload

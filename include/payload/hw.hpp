#pragma once

/**
 * @file hw.hpp
 * @brief DeviceTree-derived payload hardware configuration.
 */

#include "payload/payload_types.hpp"

#include <zephyr/device.h>

#include <cstdint>

namespace payload
{

/*
 * The array index is the AMU's payload index.  face_bit is generated directly
 * from the shared DeviceTree `face` enum; C++ does not duplicate face names.
 */
struct AmuConfig {
	const struct device *dev;
	uint32_t face_bit;
};

extern AmuConfig amus[NUM_AMUS];

/* Null when the `imu` alias is missing or the node is disabled. */
extern const struct device *imu_dev;

/* Populate amus[payload-index] from DeviceTree. Call once before sampling. */
void init_amus();

} // namespace payload

#pragma once

/**
 * @file payload_types.hpp
 * @brief Payload data types and DeviceTree-derived payload sizing.
 */

#include <amu.h>

#include <zephyr/devicetree.h>

#include <cstddef>

namespace payload
{

// AMU count is done via amount of them marked as "okay" in DT
inline constexpr size_t NUM_AMUS = DT_NUM_INST_STATUS_OKAY(aerospace_amu);

/*
 * The on-disk format reserves bit 0 for the IMU and one bit per AMU after it,
 * so a uint32_t file mask can represent at most 31 AMUs.
 */
static_assert(NUM_AMUS > 0, "payload requires at least one AMU");
static_assert(NUM_AMUS <= 31, "payload file mask supports at most 31 AMUs");

/* One cell sweep: the two buffers returned by the AMU driver. */
struct iv_sweep_t {
	amu_sweep_meta_t meta;
	amu_sweep_iv_t iv;
};

} // namespace payload

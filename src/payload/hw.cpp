#include "payload/hw.hpp"

/**
 * @file hw.cpp
 * @brief Build device reference data structures from DeviceTree
 */

#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#include <cstddef>
#include <cstdint>

namespace payload
{

// IMU
#if DT_HAS_ALIAS(imu) && DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(imu))
const struct device *imu_dev = DEVICE_DT_GET(DT_ALIAS(imu));
#else
const struct device *imu_dev = nullptr;
#endif

/*
 * Static assertions to ensure DeviceTree was set up correctly
 */

// Ensure all AMU indices are within the amount of AMUs that there are (range check)
#define ASSERT_AMU_INDEX_IN_RANGE(node_id)                                                         \
	static_assert(DT_PROP(node_id, payload_index) < NUM_AMUS,                                  \
		      "AMU payload-index must be in the range 0..NUM_AMUS-1");

DT_FOREACH_STATUS_OKAY(aerospace_amu, ASSERT_AMU_INDEX_IN_RANGE)
#undef ASSERT_AMU_INDEX_IN_RANGE

// All AMU indices are contiguous and there aren't duplicates
#define AMU_INDEX_BIT(node_id) | BIT(DT_PROP(node_id, payload_index))

static constexpr uint32_t kDeclaredAmuIndexMask =
	0u DT_FOREACH_STATUS_OKAY(aerospace_amu, AMU_INDEX_BIT);

#undef AMU_INDEX_BIT

static_assert(kDeclaredAmuIndexMask == BIT_MASK(NUM_AMUS),
	      "AMU payload-index values must be unique and contiguous from 0");

// A face enum entry maps directly to one bit.
#define ASSERT_AMU_FACE_FITS_MASK(node_id)                                                         \
	static_assert(DT_ENUM_IDX(node_id, face) < 32, "AMU face enum exceeds face mask");

DT_FOREACH_STATUS_OKAY(aerospace_amu, ASSERT_AMU_FACE_FITS_MASK)

#undef ASSERT_AMU_FACE_FITS_MASK

/*
 * Actual AMU device gathering
 */

// Create the actual AMU list in compile-time
AmuConfig amus[NUM_AMUS] = {};

void init_amus()
{
#define INIT_AMU(node_id)                                                                          \
	do {                                                                                       \
		constexpr size_t index = DT_PROP(node_id, payload_index);                          \
		amus[index] = {                                                                    \
			.dev = DEVICE_DT_GET(node_id),                                             \
			.face_bit = static_cast<uint32_t>(BIT(DT_ENUM_IDX(node_id, face))),        \
		};                                                                                 \
	} while (0);

	DT_FOREACH_STATUS_OKAY(aerospace_amu, INIT_AMU)

#undef INIT_AMU
}

} // namespace payload

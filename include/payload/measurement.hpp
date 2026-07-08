#pragma once

/**
 * @file measurement.hpp
 * @brief Measurement routines for payload devices.
 *
 * Implements the policy for collecting data from configured devices (including
 * per-face sweep gating) and populating the `*Reading` structs defined in
 * `payload_types.hpp`.
 */

#include "payload/payload_types.hpp"

namespace payload
{

void measure_face(const FacePayload &face, FaceReading &out);
void measure_imu(ImuReading &out);

} // namespace payload

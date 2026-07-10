#pragma once

/**
 * @file hw.hpp
 * @brief DeviceTree-bound slot tables for payload hardware.
 *
 * Exposes `face_z` / `face_x` which map the configured hardware into stable
 * in-memory `FacePayload` tables. These tables are consumed by measurement code
 * and the payload thread.
 */

#include "payload/payload_types.hpp"

namespace payload
{

/* Defined in hw.cpp from DeviceTree slot tables. */
extern const FacePayload face_z;
extern const FacePayload face_x;

} // namespace payload

#pragma once

#include <cstdint>

namespace payload
{

/*
 * Error bits the Payload thread can OR into its heartbeat.
 *
 * Critical bits force the thread to Dead status; non-critical bits leave it
 * Partial. Keep these stable: their numeric values travel in heartbeats and
 * are interpreted by health::Watchdog.
 */
constexpr uint32_t ErrAmuTimeout = (1u << 0);    /* one AMU stopped responding (partial loss) */
constexpr uint32_t ErrImuBadData = (1u << 1);    /* IMU returned implausible values */
constexpr uint32_t ErrLittlefsWrite = (1u << 2); /* couldn't persist sample to flash (critical) */

constexpr uint32_t CriticalErrors = ErrLittlefsWrite;

} // namespace payload

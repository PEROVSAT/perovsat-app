#pragma once

#include <zephyr/kernel.h>

#include <cstdint>

namespace sys
{

/*
 * System operating mode -- the "op_status" Global Flag the PDR flowcharts branch
 * on. Threads read it each epoch to decide how much work to attempt. Ordered by
 * how much resource/energy budget it implies:
 *
 *   SafeLow  - power/health constrained: do the minimum, preserve the vehicle.
 *   Nominal  - normal operations: run the baseline processing path.
 *   High     - surplus resources: run the heavier, higher-value analysis path.
 */
enum class OpStatus : uint32_t {
	SafeLow = 0,
	Nominal,
	High,
};

/*
 * Read/publish the current operating mode.
 *
 * Backed by a single atomic word (see global_flags.cpp), so any thread may read
 * it lock-free and a producer (System Health / FDIR / Commands) may update it
 * without coordinating with readers. This is the concrete "Global Flags" store
 * the DFA flowchart reads from.
 */
OpStatus op_status(void);
void set_op_status(OpStatus status);

} // namespace sys

#pragma once

#include "watchdog.hpp"

#include <cstdint>

/*
 * Fault Detection, Isolation and Recovery (FDIR) interface.
 *
 * ISupervised is a classic C++ interface: every concrete instance carries a
 * RAM-resident vptr, and each on_fault() call dispatches through the vtable.
 * In a radiation environment that vptr is an SEU-soft spot -- a single upset
 * flipping a bit in the vptr can redirect a virtual call to garbage. The
 * harder flight variant replaces the vtable with a const function-pointer
 * table living in .rodata/flash (immune to SEU in RAM), selected by a plain
 * index rather than a corruptible pointer. Left as future work.
 */

namespace health
{

/* What the FDIR layer decides to do about a degradation, in rising severity. */
enum class RecoveryAction : uint8_t {
	LogOnly,
	RestartThread,
	EnterSafeMode,
	ForceReset,
};

/*
 * One supervised subsystem. Implemented by a file-scope instance per monitored
 * thread; the watchdog holds a pointer to it and calls on_fault() on the first
 * poll that observes a degradation transition.
 */
class ISupervised
{
      public:
	virtual const char *name() const = 0;

	/* Decide the recovery action for a freshly-degraded subsystem. */
	virtual RecoveryAction on_fault(HealthStatus new_status, uint32_t errors) = 0;

      protected:
	/* Non-virtual: instances are never owned/deleted through this interface. */
	~ISupervised() = default;
};

/* The single recovery switch, shared by every supervised subsystem. */
void dispatch_recovery(MonitoredThread id, RecoveryAction action);

/*
 * File-scope supervised instances (concrete objects defined in health_fdir.cpp
 * with internal linkage; exposed here by reference for the arm() wire-up).
 */
extern ISupervised &payload_supervised;
extern ISupervised &dfa_supervised;
extern ISupervised &comms_supervised;
extern ISupervised &commands_supervised;

} // namespace health

#include "health_fdir.hpp"

#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(health_fdir, LOG_LEVEL_INF);

namespace health
{

namespace
{

/*
 * One final ISupervised subclass per monitored thread. Each is a file-scope
 * static instance: no heap, no dynamic dispatch beyond the vtable itself.
 */

class PayloadSupervised final: public ISupervised
{
      public:
	const char *name() const override
	{
		return "payload";
	}

	RecoveryAction on_fault(HealthStatus new_status, uint32_t errors) override
	{
		(void)errors;
		/* A dead payload thread is restarted; a partial one only logs. */
		return new_status == HealthStatus::Dead ? RecoveryAction::RestartThread
							: RecoveryAction::LogOnly;
	}
};

class DfaSupervised final: public ISupervised
{
      public:
	const char *name() const override
	{
		return "dfa";
	}

	RecoveryAction on_fault(HealthStatus, uint32_t) override
	{
		return RecoveryAction::LogOnly;
	}
};

class CommsSupervised final: public ISupervised
{
      public:
	const char *name() const override
	{
		return "comms";
	}

	RecoveryAction on_fault(HealthStatus new_status, uint32_t errors) override
	{
		(void)errors;
		/* Loss of comms is safe-mode territory: preserve the vehicle and
		 * wait for ground. */
		return new_status == HealthStatus::Dead ? RecoveryAction::EnterSafeMode
							: RecoveryAction::LogOnly;
	}
};

class CommandsSupervised final: public ISupervised
{
      public:
	const char *name() const override
	{
		return "commands";
	}

	RecoveryAction on_fault(HealthStatus, uint32_t) override
	{
		return RecoveryAction::LogOnly;
	}
};

PayloadSupervised payload_supervised_impl;
DfaSupervised dfa_supervised_impl;
CommsSupervised comms_supervised_impl;
CommandsSupervised commands_supervised_impl;

} // namespace

ISupervised &payload_supervised = payload_supervised_impl;
ISupervised &dfa_supervised = dfa_supervised_impl;
ISupervised &comms_supervised = comms_supervised_impl;
ISupervised &commands_supervised = commands_supervised_impl;

void dispatch_recovery(MonitoredThread id, RecoveryAction action)
{
	const unsigned tid = static_cast<unsigned>(id);

	switch (action) {
	case RecoveryAction::LogOnly:
		LOG_WRN("fdir: thread %u degraded (log only)", tid);
		break;

	case RecoveryAction::RestartThread:
		LOG_ERR("fdir: thread %u fault latched; restart requested", tid);
		/* TODO: K_THREAD_DEFINE threads cannot be trivially restarted after
		 * k_thread_abort -- their stack/TCB are statically bound and there is
		 * no clean re-create. A real restart needs a re-creatable thread with
		 * a dedicated stack we re-initialize. For now the fault is only
		 * latched (observable via watchdog.status()); do NOT abort/restart. */
		break;

	case RecoveryAction::EnterSafeMode:
		LOG_ERR("fdir: thread %u fault; entering safe mode", tid);
		/* TODO: hook DFA into a commanded safe state (detumble hold / power
		 * survival) once that state machine exists. */
		break;

	case RecoveryAction::ForceReset:
		LOG_ERR("fdir: thread %u fault; forcing cold reboot", tid);
		sys_reboot(SYS_REBOOT_COLD);
		break;
	}
}

} // namespace health

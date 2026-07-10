#pragma once

namespace comms
{

/*
 * Wake signal, DFA -> Communications.
 *
 * DFA gives this after committing a processed batch to its outbox so the Comms
 * thread can attempt a downlink promptly instead of polling the filesystem on a
 * timer ("Wake Communications" in the DFA flowchart). Idempotent: repeated wakes
 * before Comms services one collapse into a single pending signal. Defined in
 * communications.cpp.
 */
void wake(void);

} // namespace comms

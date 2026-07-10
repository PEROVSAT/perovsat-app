#pragma once

#include "global_flags.hpp"

#include <cstdint>

/*
 * Data Filtering & Analysis tasks.
 *
 * Free functions rather than a class: the DFA thread owns no devices, and these
 * are just a grouping of pipeline steps. Their small amount of cross-pass state
 * -- the cumulative sun-exposure accumulator and this pass's reap bookkeeping --
 * lives as file-scope statics in tasks.cpp, touched only by the single DFA
 * thread. dfa.cpp stays a thin orchestrator that sequences these per the PDR
 * flowchart.
 */
namespace dfa
{

/* Restore persistent state (cumulative sun exposure) from LittleFS. */
int init();

/*
 * Run one analysis pass for the given operating mode: fetch new payload data,
 * apply the mode-appropriate transform, fold in cumulative sun exposure, and
 * save the result to the outbox. Returns OR-accumulated dfa_err bits (0 on
 * success, or when there was simply nothing to do).
 */
uint32_t process(sys::OpStatus mode);

/* Delete raw payload data already folded into the outbox. */
uint32_t reap_old();

/*
 * Whether the most recent process() durably committed a new batch to the outbox
 * (i.e. there is something for Communications to downlink).
 */
bool produced_output();

} // namespace dfa

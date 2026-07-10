#pragma once

#include "global_flags.hpp"

#include <cstddef>
#include <cstdint>

/*
 * Data Filtering & Analysis worker.
 *
 * The DFA thread owns no devices ("Resource Ownership: None"): its inputs are
 * the raw payload records on LittleFS plus the op_status Global Flag, and its
 * output is a processed batch written back to LittleFS for Communications to
 * downlink. dfa.cpp is a thin orchestrator that walks the PDR flowchart; every
 * task with real work lives here so the run loop stays readable.
 *
 * One instance is constructed on the DFA thread's stack and lives for the life
 * of the thread; it is not shared, so its members need no locking.
 */
class DfaProcessor
{
      public:
	/* Restore persistent state (cumulative sun exposure) from LittleFS. */
	int init();

	/*
	 * Run one analysis pass for the given operating mode: fetch new payload
	 * data, apply the mode-appropriate transform, fold in cumulative sun
	 * exposure, and save the result to the outbox. Returns OR-accumulated
	 * dfa_err bits (0 on success, or when there was simply nothing to do).
	 */
	uint32_t process(sys::OpStatus mode);

	/* Delete raw payload data already folded into the outbox. */
	uint32_t reap_old();

	/* Whether the most recent process() durably committed a new batch to the
	 * outbox (i.e. there is something for Communications to downlink). */
	bool produced_output() const
	{
		return produced_;
	}

      private:
	/* Shared pipeline stages (both NOMINAL and HIGH paths). */
	int fetch_new_payload();        /* -> record count fetched, or <0 on error */
	void accumulate_sun_exposure(); /* Calculate Cumulative Sun Exposure per face */
	int save_new_data();            /* -> 0 on success */

	/* NOMINAL-mode transform. */
	void basic_filter(); /* Basic Filtering (Method TBD) */

	/* HIGH-mode transforms. */
	void iv_curve_fit(); /* (Tentative) IV Curve Fit */
	void compress();     /* (Tentative) Data Compression */

	/*
	 * A CubeSat presents six external faces; sun exposure accumulates across
	 * the whole mission, so it is checkpointed to LittleFS (see init()) and a
	 * reset must not silently zero it.
	 */
	static constexpr size_t FaceCount = 6;
	float sun_exposure_[FaceCount] = {};

	/*
	 * Raw records that have been durably committed to the outbox and are now
	 * safe to delete. Set only AFTER a successful save so a failed save (or a
	 * crash before save) never causes reap_old() to drop un-saved raw data.
	 */
	int reapable_records_ = 0;

	/* Set by process(): did the last pass put a new batch in the outbox? */
	bool produced_ = false;
};

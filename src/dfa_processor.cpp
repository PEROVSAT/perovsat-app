#include "dfa.hpp"
#include "watchdog.hpp"

#include <zephyr/logging/log.h>

/* Share the "dfa" log module registered in dfa.cpp rather than opening a second. */
LOG_MODULE_DECLARE(dfa, LOG_LEVEL_DBG);

int DfaProcessor::init()
{
	/*
	 * TODO(littlefs): restore the cumulative sun-exposure checkpoint from the
	 * storage partition so a reset does not zero mission-long totals. Until the
	 * fs:: layer lands we boot from a clean accumulator, which is safe (only
	 * loses history across a reset, never corrupts).
	 */
	for (size_t i = 0; i < FaceCount; ++i) {
		sun_exposure_[i] = 0.0f;
	}
	reapable_records_ = 0;
	produced_ = false;
	LOG_INF("DfaProcessor init: sun-exposure accumulator cleared (%u faces)",
		(unsigned)FaceCount);
	return 0;
}

uint32_t DfaProcessor::process(sys::OpStatus mode)
{
	/* Every pass starts owing nothing until a save succeeds below. */
	produced_ = false;

	/* Fetch all new payload data (common entry to both branches). */
	const int fetched = fetch_new_payload();
	if (fetched < 0) {
		LOG_ERR("fetch failed (%d)", fetched);
		return health::dfa_err::FetchFail;
	}
	if (fetched == 0) {
		LOG_DBG("no new payload data this epoch");
		return 0; /* nothing to do is not a fault */
	}
	LOG_INF("processing %d new payload record(s) in %s mode", fetched,
		mode == sys::OpStatus::High ? "HIGH" : "NOMINAL");

	/* Mode-specific transform (the HIGH/NOMINAL split in the flowchart). */
	if (mode == sys::OpStatus::High) {
		iv_curve_fit(); /* (Tentative) */
		compress();     /* (Tentative) */
	} else {
		basic_filter(); /* Method TBD */
	}

	/* Shared: fold this batch into cumulative sun exposure per face. */
	accumulate_sun_exposure();

	/*
	 * Save the processed batch to the outbox for Communications. Only on a
	 * durable save do we (a) mark the raw records reapable and (b) report that
	 * output was produced -- a failed save leaves the raw data intact so the
	 * next pass retries it instead of losing it.
	 */
	if (save_new_data() != 0) {
		LOG_ERR("save to outbox failed; leaving raw data for retry");
		return health::dfa_err::SaveFail;
	}
	reapable_records_ = fetched;
	produced_ = true;

	return 0;
}

uint32_t DfaProcessor::reap_old()
{
	if (reapable_records_ == 0) {
		return 0; /* nothing durably saved this pass -> nothing safe to delete */
	}

	/*
	 * TODO(littlefs): delete the raw payload records that were folded into the
	 * outbox. Reaching here means save_new_data() already committed them, so a
	 * crash between save and reap re-processes rather than loses data.
	 */
	LOG_DBG("reaping %d consumed raw record(s)", reapable_records_);
	reapable_records_ = 0;
	return 0;
}

int DfaProcessor::fetch_new_payload()
{
	/*
	 * TODO(littlefs): read the Payload thread's outbox (raw records) into a
	 * working buffer and return the count. Returns 0 until the fs:: append-log
	 * reader lands, so the pipeline is inert-but-correct rather than fabricating
	 * data.
	 */
	return 0;
}

void DfaProcessor::basic_filter()
{
	/* TODO: Basic Filtering (Method TBD in PDR) -- e.g. outlier rejection. */
	LOG_DBG("basic filtering (stub)");
}

void DfaProcessor::iv_curve_fit()
{
	/* TODO: (Tentative) IV curve fit -- extract Voc/Isc/Pmax/fill-factor. */
	LOG_DBG("IV curve fit (stub)");
}

void DfaProcessor::compress()
{
	/* TODO: (Tentative) data compression before downlink. */
	LOG_DBG("data compression (stub)");
}

void DfaProcessor::accumulate_sun_exposure()
{
	/*
	 * TODO: integrate this batch's illumination into sun_exposure_[face] and
	 * checkpoint the running totals to LittleFS. Stubbed until the record format
	 * exposes per-face illumination.
	 */
	LOG_DBG("accumulate cumulative sun exposure per face (stub)");
}

int DfaProcessor::save_new_data()
{
	/*
	 * TODO(littlefs): append the processed batch to the DFA outbox and give
	 * comms::wake() only after it is durably committed (the caller wakes Comms).
	 */
	LOG_DBG("save processed batch to outbox (stub)");
	return 0;
}

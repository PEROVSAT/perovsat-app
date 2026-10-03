#pragma once

#include <zephyr/sys/atomic.h>

extern atomic_t boot_count;
extern atomic_t op_status;
/*
This should be atomic because our OBC is multicore, and we'll want to use them both.
If a reader core tries to access it at the same time a writer core (system health) is
updating it, then that results in undefined behavior.

At the same time, we also want to have TMR on this, as its possibly the MOST critical variable
*/

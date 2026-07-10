#include "global_flags.hpp"

namespace sys
{

namespace
{

/*
 * The single backing word for op_status. Boots to Nominal; producers move it as
 * conditions change. atomic_t makes reads and writes indivisible across threads
 * with no lock, which is all a one-word flag needs.
 */
atomic_t op_status_word = ATOMIC_INIT(static_cast<atomic_val_t>(OpStatus::Nominal));

} // namespace

OpStatus op_status(void)
{
	return static_cast<OpStatus>(atomic_get(&op_status_word));
}

void set_op_status(OpStatus status)
{
	atomic_set(&op_status_word, static_cast<atomic_val_t>(status));
}

} // namespace sys

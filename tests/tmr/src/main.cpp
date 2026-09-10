/**
 * @file main.cpp
 * @brief Exhaustive ztest coverage for Tmr<>.
 */

#include "tmr.hpp"

#include <zephyr/ztest.h>

enum class Mode : unsigned char {
	Off = 0,
	On = 1,
	Fault = 2,
};

template <typename T> static void assert_replicas(const Tmr<T> &tmr, T a, T b, T c)
{
	zassert_equal(tmr.get_replica(0), a, "replica 0");
	zassert_equal(tmr.get_replica(1), b, "replica 1");
	zassert_equal(tmr.get_replica(2), c, "replica 2");
}

template <typename T> static void assert_unanimous(const Tmr<T> &tmr, T expected)
{
	assert_replicas(tmr, expected, expected, expected);
	zassert_equal(tmr.read(), expected, "voted value");
}

ZTEST_SUITE(tmr, NULL, NULL, NULL, NULL, NULL);

ZTEST(tmr, test_default_construct_zero)
{
	Tmr<int> n;

	assert_unanimous(n, 0);
}

ZTEST(tmr, test_value_construct)
{
	Tmr<int> n = 42;

	assert_unanimous(n, 42);
}

ZTEST(tmr, test_assign_from_t)
{
	Tmr<int> n = 1;

	n = 7;
	assert_unanimous(n, 7);
}

ZTEST(tmr, test_assign_overwrites_disagreement)
{
	Tmr<int> n = 1;

	n.set_replica(0, 9);
	n.set_replica(1, 8);
	n.set_replica(2, 7);
	n = 4;
	assert_unanimous(n, 4);
}

ZTEST(tmr, test_assign_returns_this)
{
	Tmr<int> n = 0;

	zassert_equal_ptr(&(n = 3), &n);
	assert_unanimous(n, 3);
}

ZTEST(tmr, test_copy_construct_copies_replicas)
{
	Tmr<int> src = 5;
	Tmr<int> dst = src;

	assert_unanimous(dst, 5);
}

ZTEST(tmr, test_copy_construct_preserves_disagreement)
{
	Tmr<int> src = 5;

	src.set_replica(2, 9);
	Tmr<int> dst = src;
	assert_replicas(dst, 5, 5, 9);
}

ZTEST(tmr, test_copy_assign_preserves_disagreement)
{
	Tmr<int> src = 1;
	Tmr<int> dst = 0;

	src.set_replica(1, 2);
	dst = src;
	assert_replicas(dst, 1, 2, 1);
}

ZTEST(tmr, test_move_construct_copies_replicas)
{
	Tmr<int> src = 3;

	src.set_replica(2, 8);
	Tmr<int> dst = static_cast<Tmr<int> &&>(src);
	assert_replicas(dst, 3, 3, 8);
}

ZTEST(tmr, test_move_assign_copies_replicas)
{
	Tmr<int> src = 3;
	Tmr<int> dst = 0;

	src.set_replica(0, 1);
	dst = static_cast<Tmr<int> &&>(src);
	assert_replicas(dst, 1, 3, 3);
}

ZTEST(tmr, test_read_unanimous)
{
	Tmr<int> n = 10;

	zassert_equal(n.read(), 10);
	assert_replicas(n, 10, 10, 10);
}

ZTEST(tmr, test_vote_repair_c_when_ab_agree)
{
	Tmr<int> n = 10;

	n.set_replica(2, 99);
	zassert_equal(n.read(), 10);
	assert_unanimous(n, 10);
}

ZTEST(tmr, test_vote_repair_b_when_ac_agree)
{
	Tmr<int> n = 10;

	n.set_replica(1, 99);
	zassert_equal(n.read(), 10);
	assert_unanimous(n, 10);
}

ZTEST(tmr, test_vote_repair_a_when_bc_agree)
{
	Tmr<int> n = 10;

	n.set_replica(0, 99);
	zassert_equal(n.read(), 10);
	assert_unanimous(n, 10);
}

ZTEST(tmr, test_no_majority_returns_a_without_repair)
{
	Tmr<int> n = 1;

	n.set_replica(0, 1);
	n.set_replica(1, 2);
	n.set_replica(2, 3);
	zassert_equal(n.read(), 1);
	assert_replicas(n, 1, 2, 3);
}

ZTEST(tmr, test_vote_matrix)
{
	struct Case {
		int a;
		int b;
		int c;
		int voted;
		int ra;
		int rb;
		int rc;
	};

	const Case cases[] = {
		{5, 5, 5, 5, 5, 5, 5},       {5, 5, 9, 5, 5, 5, 5},  {5, 9, 5, 5, 5, 5, 5},
		{9, 5, 5, 5, 5, 5, 5},       {1, 2, 3, 1, 1, 2, 3},  {0, 0, 1, 0, 0, 0, 0},
		{-1, -1, 7, -1, -1, -1, -1}, {4, -4, 4, 4, 4, 4, 4},
	};

	for (const Case &c : cases) {
		Tmr<int> n = c.a;
		n.set_replica(0, c.a);
		n.set_replica(1, c.b);
		n.set_replica(2, c.c);
		zassert_equal(n.read(), c.voted, "voted value");
		assert_replicas(n, c.ra, c.rb, c.rc);
	}
}

ZTEST(tmr, test_const_read_scrubs)
{
	Tmr<int> n = 7;

	n.set_replica(1, 0);
	const Tmr<int> &cref = n;
	zassert_equal(cref.read(), 7);
	assert_unanimous(n, 7);
}

ZTEST(tmr, test_compound_add)
{
	Tmr<int> n = 10;

	n += 5;
	assert_unanimous(n, 15);
}

ZTEST(tmr, test_compound_sub)
{
	Tmr<int> n = 10;

	n -= 3;
	assert_unanimous(n, 7);
}

ZTEST(tmr, test_compound_mul)
{
	Tmr<int> n = 6;

	n *= 7;
	assert_unanimous(n, 42);
}

ZTEST(tmr, test_compound_div)
{
	Tmr<int> n = 20;

	n /= 4;
	assert_unanimous(n, 5);
}

ZTEST(tmr, test_compound_mod)
{
	Tmr<int> n = 17;

	n %= 5;
	assert_unanimous(n, 2);
}

ZTEST(tmr, test_compound_and)
{
	Tmr<unsigned> n = 0b1100u;

	n &= 0b1010u;
	assert_unanimous(n, 0b1000u);
}

ZTEST(tmr, test_compound_or)
{
	Tmr<unsigned> n = 0b0100u;

	n |= 0b0011u;
	assert_unanimous(n, 0b0111u);
}

ZTEST(tmr, test_compound_xor)
{
	Tmr<unsigned> n = 0b1100u;

	n ^= 0b1010u;
	assert_unanimous(n, 0b0110u);
}

ZTEST(tmr, test_compound_shl)
{
	Tmr<unsigned> n = 1u;

	n <<= 3u;
	assert_unanimous(n, 8u);
}

ZTEST(tmr, test_compound_shr)
{
	Tmr<unsigned> n = 8u;

	n >>= 2u;
	assert_unanimous(n, 2u);
}

ZTEST(tmr, test_compound_returns_this)
{
	Tmr<int> n = 1;

	zassert_equal_ptr(&(n += 2), &n);
	zassert_equal_ptr(&(n -= 1), &n);
	zassert_equal_ptr(&(n *= 4), &n);
	zassert_equal_ptr(&(n /= 2), &n);
	zassert_equal_ptr(&(n %= 3), &n);
	assert_unanimous(n, 1);
}

ZTEST(tmr, test_chained_compound)
{
	Tmr<int> n = 1;

	(n += 2) += 3;
	assert_unanimous(n, 6);
}

ZTEST(tmr, test_prefix_increment)
{
	Tmr<int> n = 5;

	zassert_equal_ptr(&(++n), &n);
	assert_unanimous(n, 6);
}

ZTEST(tmr, test_postfix_increment)
{
	Tmr<int> n = 5;
	const int old = n++;

	zassert_equal(old, 5);
	assert_unanimous(n, 6);
}

ZTEST(tmr, test_prefix_decrement)
{
	Tmr<int> n = 5;

	zassert_equal_ptr(&(--n), &n);
	assert_unanimous(n, 4);
}

ZTEST(tmr, test_postfix_decrement)
{
	Tmr<int> n = 5;
	const int old = n--;

	zassert_equal(old, 5);
	assert_unanimous(n, 4);
}

ZTEST(tmr, test_operators_vote_before_apply)
{
	for (unsigned i = 0; i < 3; ++i) {
		Tmr<int> n = 10;
		n.set_replica(i, 1000);
		n += 1;
		assert_unanimous(n, 11);
	}
}

ZTEST(tmr, test_increment_votes_before_apply)
{
	for (unsigned i = 0; i < 3; ++i) {
		Tmr<int> n = 10;
		n.set_replica(i, 1000);
		++n;
		assert_unanimous(n, 11);
	}
}

ZTEST(tmr, test_postfix_increment_votes_before_apply)
{
	for (unsigned i = 0; i < 3; ++i) {
		Tmr<int> n = 10;
		n.set_replica(i, 1000);
		const int old = n++;
		zassert_equal(old, 10);
		assert_unanimous(n, 11);
	}
}

ZTEST(tmr, test_negative_arithmetic)
{
	Tmr<int> n = -3;

	n += -2;
	assert_unanimous(n, -5);
	n -= -8;
	assert_unanimous(n, 3);
}

ZTEST(tmr, test_uint8_wrap)
{
	Tmr<unsigned char> n = 255;

	++n;
	assert_unanimous<unsigned char>(n, 0);
	--n;
	assert_unanimous<unsigned char>(n, 255);
}

ZTEST(tmr, test_enum_assign_and_read)
{
	Tmr<Mode> s = Mode::Off;

	zassert_equal(s.read(), Mode::Off);
	s = Mode::On;
	assert_unanimous(s, Mode::On);
}

ZTEST(tmr, test_enum_vote_repair)
{
	Tmr<Mode> s = Mode::On;

	s.set_replica(2, Mode::Fault);
	zassert_equal(s.read(), Mode::On);
	assert_unanimous(s, Mode::On);
}

ZTEST(tmr, test_enum_no_majority)
{
	Tmr<Mode> s = Mode::Off;

	s.set_replica(0, Mode::Off);
	s.set_replica(1, Mode::On);
	s.set_replica(2, Mode::Fault);
	zassert_equal(s.read(), Mode::Off);
	assert_replicas(s, Mode::Off, Mode::On, Mode::Fault);
}

ZTEST(tmr, test_bool_assign_and_vote)
{
	Tmr<bool> flag = false;

	zassert_false(flag.read());
	flag = true;
	zassert_true(flag.read());
	flag.set_replica(1, false);
	zassert_true(flag.read());
	assert_unanimous(flag, true);
}

ZTEST(tmr, test_float_arithmetic)
{
	Tmr<float> f = 1.5f;

	f += 2.5f;
	zassert_equal(f.read(), 4.0f);
	f *= 0.5f;
	zassert_equal(f.read(), 2.0f);
}

ZTEST(tmr, test_float_vote_repair)
{
	Tmr<float> f = 1.0f;

	f.set_replica(2, 99.0f);
	zassert_equal(f.read(), 1.0f);
	zassert_equal(f.get_replica(2), 1.0f);
}

ZTEST(tmr, test_float_nan_not_equal_to_self)
{
	const float nan = __builtin_nanf("");
	Tmr<float> f = nan;

	zassert_true(__builtin_isnan(f.get_replica(0)));
	zassert_true(__builtin_isnan(f.get_replica(1)));
	zassert_true(__builtin_isnan(f.get_replica(2)));
	zassert_true(__builtin_isnan(f.read()), "NaN != NaN, so vote has no majority");
}

ZTEST(tmr, test_set_replica_ignores_out_of_range)
{
	Tmr<int> n = 6;

	n.set_replica(3, 99);
	assert_unanimous(n, 6);
	zassert_equal(n.get_replica(3), 0);
}

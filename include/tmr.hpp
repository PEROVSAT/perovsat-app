#pragma once

/**
 * @file tmr.hpp
 * @brief Voting triple modular redundancy for primitive values.
 *
 * `Tmr<T>` keeps three replicas of a value. Writes update every replica.
 * `read()` majority-votes and repairs a dissenting replica when two copies
 * agree. Mutations go through the usual compound operators (`+=`, `++`, …),
 * which vote, apply the operation, then write the result back to all three.
 *
 * There is no conversion to `T`: callers must use `read()` to observe the
 * voted value. The type is limited to arithmetic and enumeration types so
 * those operators are well-defined without mutator lambdas.
 */

namespace tmr_detail
{

template <typename T> struct is_allowed {
	static constexpr bool value = __is_enum(T);
};

#define TMR_ALLOW(TYPE)                                                                            \
	template <> struct is_allowed<TYPE> {                                                      \
		static constexpr bool value = true;                                                \
	}

TMR_ALLOW(bool);
TMR_ALLOW(char);
TMR_ALLOW(signed char);
TMR_ALLOW(unsigned char);
TMR_ALLOW(short);
TMR_ALLOW(unsigned short);
TMR_ALLOW(int);
TMR_ALLOW(unsigned int);
TMR_ALLOW(long);
TMR_ALLOW(unsigned long);
TMR_ALLOW(long long);
TMR_ALLOW(unsigned long long);
TMR_ALLOW(float);
TMR_ALLOW(double);
TMR_ALLOW(long double);
TMR_ALLOW(wchar_t);
TMR_ALLOW(char16_t);
TMR_ALLOW(char32_t);

#undef TMR_ALLOW

} // namespace tmr_detail

template <typename T> class Tmr
{
	static_assert(tmr_detail::is_allowed<T>::value,
		      "Tmr<> supports arithmetic and enum types only");

      public:
	Tmr();
	Tmr(T value);

	Tmr(const Tmr &) = default;
	Tmr(Tmr &&) = default;
	Tmr &operator=(const Tmr &) = default;
	Tmr &operator=(Tmr &&) = default;
	~Tmr() = default;

	Tmr &operator=(T value);

	operator T() const = delete;

	T read() const;

#ifdef CONFIG_ZTEST
	/* Inject or inspect a single replica. Flight builds do not compile these. */
	void set_replica(unsigned index, T value);
	T get_replica(unsigned index) const;
#endif

	Tmr &operator+=(T rhs);
	Tmr &operator-=(T rhs);
	Tmr &operator*=(T rhs);
	Tmr &operator/=(T rhs);
	Tmr &operator%=(T rhs);
	Tmr &operator&=(T rhs);
	Tmr &operator|=(T rhs);
	Tmr &operator^=(T rhs);
	Tmr &operator<<=(T rhs);
	Tmr &operator>>=(T rhs);

	Tmr &operator++();
	T operator++(int);
	Tmr &operator--();
	T operator--(int);

      private:
	void store(T value);

	/* mutable: read() is logically const but may scrub a dissenting replica. */
	mutable volatile T a_;
	mutable volatile T b_;
	mutable volatile T c_;
};

template <typename T> Tmr<T>::Tmr() : Tmr(T{})
{
}

template <typename T> Tmr<T>::Tmr(T value) : a_(value), b_(value), c_(value)
{
}

template <typename T> Tmr<T> &Tmr<T>::operator=(T value)
{
	store(value);
	return *this;
}

template <typename T> T Tmr<T>::read() const
{
	const T a = a_;
	const T b = b_;
	const T c = c_;

	// TODO: Report SEUs to system health (secondary mission: report radiation problems on cheap
	// hardware)

	if (a == b) {
		if (a != c) {
			c_ = a;
		}
		return a;
	}
	if (a == c) {
		b_ = a;
		return a;
	}
	if (b == c) {
		a_ = b;
		return b;
	}

	// TODO: Handle MBU
	return a;
}

template <typename T> void Tmr<T>::store(T value)
{
	a_ = value;
	b_ = value;
	c_ = value;
}

#ifdef CONFIG_ZTEST
// These will only be included when doing tests. We obviously don't want to be able to change just
// one value in flight
template <typename T> void Tmr<T>::set_replica(unsigned index, T value)
{
	switch (index) {
	case 0:
		a_ = value;
		break;
	case 1:
		b_ = value;
		break;
	case 2:
		c_ = value;
		break;
	default:
		break;
	}
}

template <typename T> T Tmr<T>::get_replica(unsigned index) const
{
	switch (index) {
	case 0:
		return a_;
	case 1:
		return b_;
	case 2:
		return c_;
	default:
		return T{};
	}
}
#endif

/*
 * Individual Operators
 */

template <typename T> Tmr<T> &Tmr<T>::operator+=(T rhs)
{
	T value = read();
	value += rhs;
	store(value);
	return *this;
}

template <typename T> Tmr<T> &Tmr<T>::operator-=(T rhs)
{
	T value = read();
	value -= rhs;
	store(value);
	return *this;
}

template <typename T> Tmr<T> &Tmr<T>::operator*=(T rhs)
{
	T value = read();
	value *= rhs;
	store(value);
	return *this;
}

template <typename T> Tmr<T> &Tmr<T>::operator/=(T rhs)
{
	T value = read();
	value /= rhs;
	store(value);
	return *this;
}

template <typename T> Tmr<T> &Tmr<T>::operator%=(T rhs)
{
	T value = read();
	value %= rhs;
	store(value);
	return *this;
}

template <typename T> Tmr<T> &Tmr<T>::operator&=(T rhs)
{
	T value = read();
	value &= rhs;
	store(value);
	return *this;
}

template <typename T> Tmr<T> &Tmr<T>::operator|=(T rhs)
{
	T value = read();
	value |= rhs;
	store(value);
	return *this;
}

template <typename T> Tmr<T> &Tmr<T>::operator^=(T rhs)
{
	T value = read();
	value ^= rhs;
	store(value);
	return *this;
}

template <typename T> Tmr<T> &Tmr<T>::operator<<=(T rhs)
{
	T value = read();
	value <<= rhs;
	store(value);
	return *this;
}

template <typename T> Tmr<T> &Tmr<T>::operator>>=(T rhs)
{
	T value = read();
	value >>= rhs;
	store(value);
	return *this;
}

template <typename T> Tmr<T> &Tmr<T>::operator++()
{
	T value = read();
	++value;
	store(value);
	return *this;
}

template <typename T> T Tmr<T>::operator++(int)
{
	T value = read();
	T previous = value;
	++value;
	store(value);
	return previous;
}

template <typename T> Tmr<T> &Tmr<T>::operator--()
{
	T value = read();
	--value;
	store(value);
	return *this;
}

template <typename T> T Tmr<T>::operator--(int)
{
	T value = read();
	T previous = value;
	--value;
	store(value);
	return previous;
}

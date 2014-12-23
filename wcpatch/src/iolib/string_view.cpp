#include "string_view.h"


template <typename IntType, class charT, class traits>
static IntType parse(iolib::basic_string_view<charT, traits> str, size_t* idx, int base);
#if 0
template <typename FloatType, class charT, class traits>
static FloatType parse(iolib::basic_string_view<charT, traits> str, size_t* idx);
#endif
template <typename IntType>
static bool append(IntType& value, unsigned base, unsigned digit);

namespace iolib {

int stoi(string_view str, size_t* idx, int base)
{
	return parse<int>(str, idx, base);
}

long stol(string_view str, size_t* idx, int base)
{
	return parse<long>(str, idx, base);
}

unsigned long stoul(string_view str, size_t* idx, int base)
{
	return parse<unsigned long>(str, idx, base);
}

long long stoll(string_view str, size_t* idx, int base)
{
	return parse<long long>(str, idx, base);
}

unsigned long long stoull(string_view str, size_t* idx, int base)
{
	return parse<unsigned long long>(str, idx, base);
}

#if 0
float stof(string_view str, size_t* idx)
{
	return 0.0f;
}

double stod(string_view str, size_t* idx)
{
	return 0.0;
}

long double stold(string_view str, size_t* idx)
{
	return 0.0l;
}
#endif

int stoi(wstring_view str, size_t* idx, int base)
{
	return parse<int>(str, idx, base);
}

long stol(wstring_view str, size_t* idx, int base)
{
	return parse<long>(str, idx, base);
}

unsigned long stoul(wstring_view str, size_t* idx, int base)
{
	return parse<unsigned long>(str, idx, base);
}

long long stoll(wstring_view str, size_t* idx, int base)
{
	return parse<long long>(str, idx, base);
}

unsigned long long stoull(wstring_view str, size_t* idx, int base)
{
	return parse<unsigned long long>(str, idx, base);
}

#if 0
float stof(wstring_view str, size_t* idx)
{
	return 0.0f;
}

double stod(wstring_view str, size_t* idx)
{
	return 0.0;
}

long double stold(wstring_view str, size_t* idx)
{
	return 0.0;
}
#endif

} // namespace iolib

template <typename IntType, class charT, class traits>
IntType parse(iolib::basic_string_view<charT, traits> str, size_t* idx, int base)
{
	size_t idx_dummy;
	size_t& idx_ret = idx != nullptr ? *idx : idx_dummy;

	IntType value = 0;
	bool negative = false;

	auto first = str.begin();
	auto i = first;
	auto last = str.end();

	typedef std::ctype<charT> ctype;
	auto& ct = std::use_facet<ctype>(std::locale());

	while ((i != last) && ct.is(ctype::space, *i))
		++i;

	idx_ret = i - first;

	if (i != last)
	{
		if (*i == '+')
			++i;
		else if (*i == '-')
		{
			negative = true;
			++i;
		}

		if (base == 0)
		{
			if (*i == '0')
			{
				base = 8;
				++i;

				if ((*i == 'x') || (*i == 'X'))
				{
					base = 16;
					++i;
				}
				else
					idx_ret = i - first;
			}
		}

		bool overflow = false;
		while (i != last)
		{
			bool pass_overflow = false;
			if (('0' <= *i) && (*i - '0' < base))
				pass_overflow = !append(value, base, *i - '0');
			else if (base > 10)
			{
				if (('A' <= *i) && (*i - 'A' < base - 10))
					pass_overflow = !append(value, base, *i - 'A' + 10);
				else if (('a' <= *i) && (*i - 'a' < base - 10))
					pass_overflow = !append(value, base, *i - 'a' + 10);
				else
					break;
			}
			else
				break;

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
			if (!pass_overflow && (value < 0))
#ifdef __clang__
#pragma clang diagnostic pop
#endif
				pass_overflow = negative && (value == -1 * value);

			if (pass_overflow)
				overflow = true;

			idx_ret = ++i - first;
		}

		if (negative)
			value *= -1;

		if (overflow || (negative && value > 0))
		{
			errno = ERANGE;
			throw std::out_of_range("converted value out of range");
		}
	}

	return value;
}

#if 0
template <typename FloatType, class charT, class traits>
FloatType parse(iolib::basic_string_view<charT, traits> str, size_t* idx)
{
}
#endif

template <typename IntType>
bool append(IntType& value, unsigned base, unsigned digit)
{
	typedef typename std::make_unsigned<IntType>::type UIntType;
	static const UIntType max_value = std::numeric_limits<UIntType>::max();
	static const UIntType premul_overflow = max_value / 10;

	bool overflow = UIntType(value) > premul_overflow;
	value *= base;

	overflow = overflow || (UIntType(value) > max_value - digit);
	value += digit;

	return !overflow;
}

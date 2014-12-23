#ifndef IOLIB_STRING_VIEW_INCLUDED
#define IOLIB_STRING_VIEW_INCLUDED
#pragma once

#include <algorithm>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

#include <cstddef>


namespace stdext { struct random_access_range_tag; }

namespace iolib {

template <class charT, class traits = std::char_traits<charT>>
class basic_string_view;

typedef basic_string_view<char>     string_view;
typedef basic_string_view<wchar_t> wstring_view;

template <class charT, class traits> bool operator == (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept;
template <class charT, class traits> bool operator == (const basic_string_view<charT, traits>& lhs, const charT* rhs);
template <class charT, class traits> bool operator == (const charT* lhs, const basic_string_view<charT, traits>& rhs);
template <class charT, class traits> bool operator != (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept;
template <class charT, class traits> bool operator != (const basic_string_view<charT, traits>& lhs, const charT* rhs);
template <class charT, class traits> bool operator != (const charT* lhs, const basic_string_view<charT, traits>& rhs);

template <class charT, class traits> bool operator < (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept;
template <class charT, class traits> bool operator < (const basic_string_view<charT, traits>& lhs, const charT* rhs);
template <class charT, class traits> bool operator < (const charT* lhs, const basic_string_view<charT, traits>& rhs);
template <class charT, class traits> bool operator > (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept;
template <class charT, class traits> bool operator > (const basic_string_view<charT, traits>& lhs, const charT* rhs);
template <class charT, class traits> bool operator > (const charT* lhs, const basic_string_view<charT, traits>& rhs);

template <class charT, class traits> bool operator <= (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept;
template <class charT, class traits> bool operator <= (const basic_string_view<charT, traits>& lhs, const charT* rhs);
template <class charT, class traits> bool operator <= (const charT* lhs, const basic_string_view<charT, traits>& rhs);
template <class charT, class traits> bool operator >= (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept;
template <class charT, class traits> bool operator >= (const basic_string_view<charT, traits>& lhs, const charT* rhs);
template <class charT, class traits> bool operator >= (const charT* lhs, const basic_string_view<charT, traits>& rhs);

template <class charT, class traits> void swap(basic_string_view<charT, traits>& lhs, basic_string_view<charT, traits>& rhs);

template <class charT, class traits> std::basic_ostream<charT, traits>& operator << (std::basic_ostream<charT, traits>& os, const basic_string_view<charT, traits>& str);

int stoi(string_view str, size_t* idx = nullptr, int base = 10);
long stol(string_view str, size_t* idx = nullptr, int base = 10);
unsigned long stoul(string_view str, size_t* idx = nullptr, int base = 10);
long long stoll(string_view str, size_t* idx = nullptr, int base = 10);
unsigned long long stoull(string_view str, size_t* idx = nullptr, int base = 10);
#if 0
float stof(string_view str, size_t* idx = nullptr);
double stod(string_view str, size_t* idx = nullptr);
long double stold(string_view str, size_t* idx = nullptr);
#endif

int stoi(wstring_view str, size_t* idx = nullptr, int base = 10);
long stol(wstring_view str, size_t* idx = nullptr, int base = 10);
unsigned long stoul(wstring_view str, size_t* idx = nullptr, int base = 10);
long long stoll(wstring_view str, size_t* idx = nullptr, int base = 10);
unsigned long long stoull(wstring_view str, size_t* idx = nullptr, int base = 10);
#if 0
float stof(wstring_view str, size_t* idx = nullptr);
double stod(wstring_view str, size_t* idx = nullptr);
long double stold(wstring_view str, size_t* idx = nullptr);
#endif


template <class charT, class traits>
class basic_string_view
{
public:
	typedef traits                     traits_type;
	typedef typename traits::char_type value_type;
	typedef std::size_t                size_type;
	typedef std::ptrdiff_t             difference_type;

	typedef const value_type&               reference;
	typedef const value_type*               pointer;
	typedef pointer                         iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;

	typedef stdext::random_access_range_tag range_category;
	typedef iterator                        position_type;

public:
	static const size_type npos = -1;

public:
	basic_string_view() : first(nullptr), last(nullptr) { }
	basic_string_view(const basic_string_view& str) : first(str.first), last(str.last) { }
	basic_string_view(basic_string_view&& str) : first(std::move(str.first)), last(std::move(str.last)) { }
	basic_string_view(const basic_string_view& str, size_type pos, size_type n = npos) : first(str.first + pos), last(n == npos ? str.last : first + n) { /* throw if pos or n invalid */ }
	basic_string_view(pointer str, size_type n) : first(str), last(first + n) { }
	basic_string_view(pointer str) : first(str), last(first + traits_type::length(str)) { }
	basic_string_view(pointer begin, pointer end) : first(begin), last(end) { /* throw if end < begin */ }
	~basic_string_view() { }
	template <class Allocator>
	basic_string_view(const std::basic_string<charT, traits, Allocator>& str) { assign(str); }
	basic_string_view& operator = (pointer str) { return assign(str); }
	template <class Allocator>
	basic_string_view& operator = (const std::basic_string<charT, traits, Allocator>& str) { return assign(str); }

public:
	iterator begin() const noexcept { return first; }
	iterator end() const noexcept   { return last; }
	reverse_iterator rbegin() const noexcept { return reverse_iterator(last); }
	reverse_iterator rend() const noexcept   { return reverse_iterator(first); }
	position_type begin_pos() const noexcept { return begin(); }
	void begin_pos(position_type pos) { first = pos; }
	position_type end_pos() const noexcept{ return end(); }
	void end_pos(position_type pos) { last = pos; }

	size_type size() const noexcept   { return last - first; }
	size_type length() const noexcept { return last - first; }
	bool empty() const noexcept { return last == first; }

	reference operator [] (size_type pos) const { return *(first + pos); } // no range check
	reference at(size_type n) const { return *(first + n); } // throw if n out of range
	reference at_pos(const position_type& pos) const { return *pos; } // throw if p out of range
	position_type position_at(size_type n) const { return first + n; }
	size_type index_at(position_type pos) const { return pos - first; }
	reference front() const { return *first; } // throw if empty
	reference back() const { return *(last - 1); } // throw if empty

	position_type& increment_pos(position_type& pos) const { return ++pos; }
	void advance_pos(position_type& pos, difference_type n) const { pos += n; }
	difference_type distance(position_type first, position_type last) { return last - first; }

public:
	basic_string_view& assign(const basic_string_view& str) { return *this = str; }
	basic_string_view& assign(const basic_string_view& str, size_type pos, size_type n) { first = str.first + pos; last = first + n; return *this; } // throw if pos or n invalid
	basic_string_view& assign(pointer str, size_type n) { first = str; last = first + n; return *this; }
	basic_string_view& assign(pointer str) { first = str; last = first + traits_type::length(str); return *this; }
	basic_string_view& assign(pointer begin, pointer end) { first = begin; last = end; return *this; }
	template <class Allocator>
	basic_string_view& assign(const std::basic_string<charT, traits, Allocator>& str)
	{
		if (str.length() > 0)
		{
			first = &str.front();
			last = first + str.length();
		}
		else
		{
			first = nullptr;
			last = nullptr;
		}

		return *this;
	}

	void swap(basic_string_view& str) { swap(first, str.first); swap(last, str.last); }

public:
	pointer data() const noexcept { return first; }

public:
	size_type  find(const basic_string_view& str, size_type pos = 0) const noexcept;
	size_type  find(pointer str, size_type pos, size_type n) const;
	size_type  find(pointer str, size_type pos = 0) const;
	size_type  find(value_type c, size_type pos = 0) const;
	size_type rfind(const basic_string_view& str, size_type pos = 0) const noexcept;
	size_type rfind(pointer str, size_type pos, size_type n) const;
	size_type rfind(pointer str, size_type pos = 0) const;
	size_type rfind(value_type c, size_type pos = 0) const;

	size_type find_first_of(const basic_string_view& str, size_type pos = 0) const noexcept;
	size_type find_first_of(pointer str, size_type pos, size_type n) const;
	size_type find_first_of(pointer str, size_type pos = 0) const;
	size_type find_first_of(value_type c, size_type pos = 0) const;
	size_type find_last_of(const basic_string_view& str, size_type pos = 0) const noexcept;
	size_type find_last_of(pointer str, size_type pos, size_type n) const;
	size_type find_last_of(pointer str, size_type pos = 0) const;
	size_type find_last_of(value_type c, size_type pos = 0) const;

	size_type find_first_not_of(const basic_string_view& str, size_type pos = 0) const noexcept;
	size_type find_first_not_of(pointer str, size_type pos, size_type n) const;
	size_type find_first_not_of(pointer str, size_type pos = 0) const;
	size_type find_first_not_of(value_type c, size_type pos = 0) const;
	size_type find_last_not_of(const basic_string_view& str, size_type pos = 0) const noexcept;
	size_type find_last_not_of(pointer str, size_type pos, size_type n) const;
	size_type find_last_not_of(pointer str, size_type pos = 0) const;
	size_type find_last_not_of(value_type c, size_type pos = 0) const;

public:
	basic_string_view substr(size_type pos = 0, size_type n = npos) const { return basic_string_view(*this, pos, n); }

	int compare(const basic_string_view& str) const noexcept;
	int compare(size_type pos1, size_type n1, const basic_string_view& str) const;
	int compare(size_type pos1, size_type n1, const basic_string_view& str, size_type pos2, size_type n2) const;
	int compare(pointer str) const;
	int compare(size_type pos1, size_type n1, pointer str) const;
	int compare(size_type pos1, size_type n1, pointer str, size_type n2) const;

private:
	pointer first, last;
};


template <class charT, class traits>
auto basic_string_view<charT, traits>::find(const basic_string_view& str, size_type pos) const noexcept -> size_type
{
	if (pos >= length())
		return npos;

	for (auto i = first + pos, end = last - str.length(); i < end; ++i)
	{
		bool found = true;
		for (auto j = i, k = str.data(), end = j + str.size(); j < end; ++j, ++k)
		{
			if (!traits_type::eq(*j, *k))
			{
				found = false;
				break;
			}
		}

		if (found)
			return i - first;
	}

	return npos;
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find(pointer str, size_type pos, size_type n) const -> size_type
{
	return find(basic_string_view(str, n), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find(pointer str, size_type pos) const -> size_type
{
	return find(basic_string_view(str), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find(value_type c, size_type pos) const -> size_type
{
	return traits_type::find(first + pos, last - first - pos, c) - first;
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::rfind(const basic_string_view& str, size_type pos) const noexcept -> size_type
{
	pos = std::min(pos + 1, length());
	for (auto i = first + pos; i-- > first; )
	{
		bool found = true;
		for (auto j = i, k = str.data(), end = j + str.size(); j < end; ++j, ++k)
		{
			if (!traits_type::eq(*j, *k))
			{
				found = false;
				break;
			}
		}

		if (found)
			return i - first;
	}

	return npos;
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::rfind(pointer str, size_type pos, size_type n) const -> size_type
{
	return rfind(basic_string_view(str, n), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::rfind(pointer str, size_type pos) const -> size_type
{
	return rfind(basic_string_view(str), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::rfind(value_type c, size_type pos) const -> size_type
{
	for (auto i = first + std::min(last - first, pos); i-- > first; )
	{
		if (traits_type::eq(*i, c))
			return i - first;
	}

	return npos;
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_first_of(const basic_string_view& str, size_type pos) const noexcept -> size_type
{
	if (pos >= length())
		return npos;

	for (auto i = first + pos; i < last; ++i)
	{
		if (std::any_of(str.begin(), str.end(), [=](value_type ch) { return traits_type::eq(ch, *i); }))
			return i - first;
	}

	return npos;
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_first_of(pointer str, size_type pos, size_type n) const -> size_type
{
	return find_first_of(basic_string_view(str, n), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_first_of(pointer str, size_type pos) const -> size_type
{
	return find_first_of(basic_string_view(str), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_first_of(value_type c, size_type pos) const -> size_type
{
	return find(c, pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_last_of(const basic_string_view& str, size_type pos) const noexcept -> size_type
{
	pos = std::min(pos + 1, length());
	for (auto i = first + pos; i-- > first; )
	{
		if (std::any_of(str.begin(), str.end(), [=](value_type ch) { return traits_type::eq(ch, *i); }))
			return i - first;
	}

	return npos;
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_last_of(pointer str, size_type pos, size_type n) const -> size_type
{
	return find_last_of(basic_string_view(str, n), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_last_of(pointer str, size_type pos) const -> size_type
{
	return find_last_of(basic_string_view(str), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_last_of(value_type c, size_type pos) const -> size_type
{
	return rfind(c);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_first_not_of(const basic_string_view& str, size_type pos) const noexcept -> size_type
{
	if (pos >= length())
		return npos;

	for (auto i = first + pos; i != last; ++i)
	{
		if (!std::any_of(str.begin(), str.end(), [=](value_type ch) { return traits::eq(ch, *i); }))
			return i - first;
	}

	return npos;
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_first_not_of(pointer str, size_type pos, size_type n) const -> size_type
{
	return find_first_not_of(basic_string_view(str, n), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_first_not_of(pointer str, size_type pos) const -> size_type
{
	return find_first_not_of(basic_string_view(str), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_first_not_of(value_type c, size_type pos) const -> size_type
{
	return find_first_not_of(basic_string_view(&c, 1), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_last_not_of(const basic_string_view& str, size_type pos) const noexcept -> size_type
{
	pos = std::min(pos + 1, length());
	for (auto i = first + pos; i-- > first; )
	{
		if (!std::any_of(str.begin(), str.end(), [=](value_type ch) { return traits_type::eq(ch, *i); }))
			return i - first;
	}

	return npos;
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_last_not_of(pointer str, size_type pos, size_type n) const -> size_type
{
	return find_last_not_of(basic_string_view(str, n), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_last_not_of(pointer str, size_type pos) const -> size_type
{
	return find_last_not_of(basic_string_view(str), pos);
}

template <class charT, class traits>
auto basic_string_view<charT, traits>::find_last_not_of(value_type c, size_type pos) const -> size_type
{
	return find_last_not_of(basic_string_view(&c, 1), pos);
}

template <class charT, class traits>
int basic_string_view<charT, traits>::compare(const basic_string_view& str) const noexcept
{
	int result = traits_type::compare(data(), str.data(), std::min(length(), str.length()));
	if (result == 0)
		result = length() < str.length() ? -1 : length() > str.length() ? 1 : 0;

	return result;
}

template <class charT, class traits>
int basic_string_view<charT, traits>::compare(size_type pos1, size_type n1, const basic_string_view& str) const
{
	return substr(pos1, n1).compare(str);
}

template <class charT, class traits>
int basic_string_view<charT, traits>::compare(size_type pos1, size_type n1, const basic_string_view& str, size_type pos2, size_type n2) const
{
	return substr(pos1, n1).compare(str.substr(pos2, n2));
}

template <class charT, class traits>
int basic_string_view<charT, traits>::compare(pointer str) const
{
	return compare(basic_string_view(str));
}

template <class charT, class traits>
int basic_string_view<charT, traits>::compare(size_type pos1, size_type n1, pointer str) const
{
	return substr(pos1, n1).compare(basic_string_view(str));
}

template <class charT, class traits>
int basic_string_view<charT, traits>::compare(size_type pos1, size_type n1, pointer str, size_type n2) const
{
	return substr(pos1, n1).compare(basic_string_view(str, n2));
}


template <class charT, class traits>
bool operator == (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept
{
	return (lhs.length() == rhs.length()) && std::equal(lhs.begin(), lhs.end(), rhs.begin(), traits::eq);
}

template <class charT, class traits>
bool operator == (const basic_string_view<charT, traits>& lhs, const charT* rhs)
{
	return lhs == basic_string_view<charT, traits>(rhs);
}

template <class charT, class traits>
bool operator == (const charT* lhs, const basic_string_view<charT, traits>& rhs)
{
	return basic_string_view<charT, traits>(lhs) == rhs;
}

template <class charT, class traits>
bool operator != (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept
{
	return !(lhs == rhs);
}

template <class charT, class traits>
bool operator != (const basic_string_view<charT, traits>& lhs, const charT* rhs)
{
	return !(lhs == rhs);
}

template <class charT, class traits>
bool operator != (const charT* lhs, const basic_string_view<charT, traits>& rhs)
{
	return !(lhs == rhs);
}

template <class charT, class traits>
bool operator < (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept
{
	for (auto i = lhs.begin(), end = i + std::min(lhs.length(), rhs.length()), j = rhs.begin(); i < end; ++i, ++j)
	{
		if (traits::lt(*i, *j))
			return true;
		if (traits::lt(*j, *i))
			return false;
	}

	if (lhs.length() < rhs.length())
		return true;

	return false;
}

template <class charT, class traits>
bool operator < (const basic_string_view<charT, traits>& lhs, const charT* rhs)
{
	return lhs < basic_string_view<charT, traits>(rhs);
}

template <class charT, class traits>
bool operator < (const charT* lhs, const basic_string_view<charT, traits>& rhs)
{
	return basic_string_view<charT, traits>(lhs) < rhs;
}

template <class charT, class traits>
bool operator > (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept
{
	return rhs < lhs;
}

template <class charT, class traits>
bool operator > (const basic_string_view<charT, traits>& lhs, const charT* rhs)
{
	return rhs < lhs;
}

template <class charT, class traits>
bool operator > (const charT* lhs, const basic_string_view<charT, traits>& rhs)
{
	return rhs < lhs;
}

template <class charT, class traits>
bool operator <= (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept
{
	return !(rhs < lhs);
}

template <class charT, class traits>
bool operator <= (const basic_string_view<charT, traits>& lhs, const charT* rhs)
{
	return !(rhs < lhs);
}

template <class charT, class traits>
bool operator <= (const charT* lhs, const basic_string_view<charT, traits>& rhs)
{
	return !(rhs < lhs);
}

template <class charT, class traits>
bool operator >= (const basic_string_view<charT, traits>& lhs, const basic_string_view<charT, traits>& rhs) noexcept
{
	return !(lhs < rhs);
}

template <class charT, class traits>
bool operator >= (const basic_string_view<charT, traits>& lhs, const charT* rhs)
{
	return !(lhs < rhs);
}

template <class charT, class traits>
bool operator >= (const charT* lhs, const basic_string_view<charT, traits>& rhs)
{
	return !(lhs < rhs);
}

template <class charT, class traits>
void swap(basic_string_view<charT, traits>& lhs, basic_string_view<charT, traits>& rhs)
{
	lhs.swap(rhs);
}

template <class charT, class traits>
std::basic_ostream<charT, traits>& operator << (std::basic_ostream<charT, traits>& os, const basic_string_view<charT, traits>& str)
{
	typename std::basic_ostream<charT, traits>::sentry sentry(os);
	if (sentry)
	{
		auto buf = os.rdbuf();
		auto pad = [=](typename basic_string_view<charT, traits>::size_type length) -> bool
		{
			charT fill = os.fill();
			for (auto fill_count = os.width() - str.length(); fill_count-- > 0; )
			{
				if (buf->sputc(fill) == traits::eof())
				{
					os.setstate(std::ios_base::failbit);
					return false;
				}
			}
			return true;
		};

		auto pad_length = os.width() > str.length() ? os.width() - str.length() : 0;
		auto adjustfield = os.flags() & std::ios_base::adjustfield;

		if ((pad_length > 0) && (adjustfield != std::ios_base::left))
			pad(pad_length);

		if (buf->sputn(str.data(), str.length()) == traits::eof())
		{
			os.setstate(std::ios_base::failbit);
			return os;
		}

		if ((pad_length > 0) && (adjustfield == std::ios_base::left))
			pad(pad_length);

		os.width(0);
	}

	return os;
}

}

#endif

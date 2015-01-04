#ifndef MD5_INCLUDED
#define MD5_INCLUDED
#pragma once

#include "iolib/file_stream.h"

#include <initializer_list>
#include <stdint.h>


struct md5_hash;

bool operator == (const md5_hash& a, const md5_hash& b);
bool operator != (const md5_hash& a, const md5_hash& b);
bool operator <  (const md5_hash& a, const md5_hash& b);
bool operator >  (const md5_hash& a, const md5_hash& b);
bool operator <= (const md5_hash& a, const md5_hash& b);
bool operator >= (const md5_hash& a, const md5_hash& b);


struct md5_hash
{
	uint32_t a, b, c, d;

	md5_hash() = default;
	md5_hash(const void* data, size_t size);
	md5_hash(std::initializer_list<uint32_t> elems);
};

inline bool operator == (const md5_hash& a, const md5_hash& b)
{
	return a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d;
}

inline bool operator != (const md5_hash& a, const md5_hash& b)
{
	return !(a == b);
}

inline bool operator < (const md5_hash& a, const md5_hash& b)
{
	return a.d < b.d ? true
		: a.d > b.d ? false
		: a.c < b.c ? true
		: a.c > b.c ? false
		: a.b < b.b ? true
		: a.b > b.b ? false
		: a.a < b.a;
}

inline bool operator > (const md5_hash& a, const md5_hash& b)
{
	return b < a;
}

inline bool operator <= (const md5_hash& a, const md5_hash& b)
{
	return !(b < a);
}

inline bool operator >= (const md5_hash& a, const md5_hash& b)
{
	return !(a < b);
}

#endif

#ifndef COMMON_INCLUDED
#define COMMON_INCLUDED
#pragma once

#include <cstddef>


template <class T, size_t size>
constexpr size_t lengthof(T (&)[size]) noexcept
{
    return size;
}

#endif

#ifndef IOLIB_TYPES_INCLUDED
#define IOLIB_TYPES_INCLUDED
#pragma once

#include <type_traits>


#define flags_enum(flags_type) \
    inline flags_type operator ~ (flags_type f) { return static_cast<flags_type>(~std::underlying_type<flags_type>::type(f)); } \
    inline flags_type operator & (flags_type a, flags_type b) { return static_cast<flags_type>(std::underlying_type<flags_type>::type(a) & std::underlying_type<flags_type>::type(b)); } \
    inline flags_type operator | (flags_type a, flags_type b) { return static_cast<flags_type>(std::underlying_type<flags_type>::type(a) | std::underlying_type<flags_type>::type(b)); } \
    inline flags_type operator ^ (flags_type a, flags_type b) { return static_cast<flags_type>(std::underlying_type<flags_type>::type(a) ^ std::underlying_type<flags_type>::type(b)); } \
    inline flags_type operator &= (flags_type a, flags_type b) { return a = a & b; } \
    inline flags_type operator |= (flags_type a, flags_type b) { return a = a | b; } \
    inline flags_type operator ^= (flags_type a, flags_type b) { return a = a ^ b; }

namespace iolib {

#ifdef _WIN32
typedef wchar_t path_char;
#define PATH(str) L##str
#else
typedef char path_char;
#define PATH(str) str
#endif

template <typename Flags>
bool test_flags(Flags value, Flags test) { return (value & test) == test; }
template <typename Flags>
Flags set_flags(Flags value, Flags set) { return value | set; }
template <typename Flags>
Flags clear_flags(Flags value, Flags clear) { return value & ~clear; }
template <typename Flags>
Flags mask_flags(Flags value, Flags mask) { return value & mask; }


template <bool value> struct is_true;
template <> struct is_true<true> : std::true_type { };
template <> struct is_true<false> : std::false_type { };

template <bool value> struct is_false;
template <> struct is_false<true> : std::false_type { };
template <> struct is_false<false> : std::true_type { };

} // namespace iolib

#endif

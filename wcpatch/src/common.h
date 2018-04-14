#ifndef COMMON_INCLUDED
#define COMMON_INCLUDED
#pragma once

#include <memory>
#include <utility>

#include <cstddef>


#define at_scope_exit(cb) auto CONCAT(scope_exit_, __COUNTER__) = make_scope_guard(cb);
#define CONCAT(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a ## b

template <class T, size_t size>
constexpr size_t lengthof(T (&)[size]) noexcept
{
    return size;
}

template <class Callback>
class scope_guard
{
public:
    scope_guard(Callback cb) : cb(cb), valid(true) { }
    scope_guard(scope_guard&& other) : cb(std::move(other.cb)), valid(std::move(other.valid)) { other.valid = false; }
    scope_guard& operator = (scope_guard&& other) { cb = std::move(other.cb); valid = std::move(other.valid); other.valid = false; return *this; }
    ~scope_guard() { if (valid) cb(); }
private:
    scope_guard(const scope_guard& other);
    scope_guard& operator = (const scope_guard& other);

private:
    Callback cb;
    bool valid;
};

template <class Callback>
scope_guard<Callback> make_scope_guard(Callback cb)
{
    return scope_guard<Callback>(cb);
}

// VS 2013 defines this function from C++14.
#if !defined _CPPLIB_VER || _CPPLIB_VER < 600
template <class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif

#endif

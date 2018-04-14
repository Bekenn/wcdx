#ifndef COMMON_INCLUDED
#define COMMON_INCLUDED
#pragma once

#define _WIN32_WINNT _WIN32_WINNT_WINXP
#define NOMINMAX
#include <Windows.h>

#define at_scope_exit(cb) auto CONCAT(scope_exit_, __COUNTER__) = make_scope_guard(cb);
#define CONCAT(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a ## b

template <class Callback>
class scope_guard
{
public:
    scope_guard(Callback cb) : cb(cb), valid(true) { }
    scope_guard(const scope_guard& other) = delete;
    scope_guard& operator = (const scope_guard& other) = delete;
    scope_guard(scope_guard&& other) : cb(std::move(other.cb)), valid(std::move(other.valid)) { other.valid = false; }
    scope_guard& operator = (scope_guard&& other) { cb = std::move(other.cb); valid = std::move(other.valid); other.valid = false; return *this; }
    ~scope_guard() { if (valid) cb(); }

private:
    Callback cb;
    bool valid;
};

template <class Callback>
scope_guard<Callback> make_scope_guard(Callback cb)
{
    return scope_guard<Callback>(cb);
}


extern HINSTANCE DllInstance;

#endif

#ifndef WINDOW_INCLUDED
#define WINDOW_INCLUDED
#pragma once

#include "platform.h"

#include <system_error>
#include <utility>

class SmartWindow
{
public:
    SmartWindow() = default;
    SmartWindow(const SmartWindow&) = delete;
    SmartWindow& operator = (const SmartWindow&) = delete;
    SmartWindow(SmartWindow&& other) : handle(std::move(other.handle))
    {
        other.handle = nullptr;
    }
    SmartWindow& operator = (SmartWindow&& other)
    {
        if (handle != nullptr)
            ::DestroyWindow(handle);
        handle = std::move(other.handle);
        other.handle = nullptr;
        return *this;
    }
    ~SmartWindow()
    {
        if (handle != nullptr)
            ::DestroyWindow(handle);
    }

    SmartWindow(
        LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
        int x, int y, int nWidth, int nHeight,
        HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
        LPVOID lpParam
        ) : handle(::CreateWindow(lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam))
    {
        if (handle == nullptr)
            throw std::system_error(::GetLastError(), std::system_category());
    }

    SmartWindow(
        DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
        int x, int y, int nWidth, int nHeight,
        HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
        LPVOID lpParam
        ) : handle(::CreateWindowEx(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam))
    {
        if (handle == nullptr)
            throw std::system_error(::GetLastError(), std::system_category());
    }

public:
    operator HWND () const { return handle; }
    HWND Get() const { return handle; }

    void Reset(HWND window = nullptr) { handle = window; }

private:
    HWND handle = nullptr;
};

#endif

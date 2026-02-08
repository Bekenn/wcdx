//****************************************Process_Has_Title****************************************
// 
// Returns true if the current process has a window with the specified title substring.
//
// Usage:
// GetTickCount64();                               - returns the number of milliseconds.
// WCDX_INJECT_TAG                                 - a unique tag used to identify injected code in the target process.
// Process_Has_Title("Title");                     - returns true if any window of the current process has "Title" in its title.
// Pull_Byte(Size_of_Patch, Address);              - reads bytes from the target process at the specified address and returns them as a vector.
// Push_Byte(Size_of_Patch, Address, {0x90,...});  - patches bytes into the target process at the specified address. Can be used to change data values or null out code with NOPs (0x90).
// Bytes_To_String(vec);                           - converts a vector of bytes to a string, preserving null bytes.
// 
//*************************************************************************************************
#pragma once

#include <windows.h>

// Ensure Win32 APIs introduced in Vista (GetTickCount64) are declared.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#if !defined(GetTickCount64)
    // For older SDKs that may not declare GetTickCount64, declare it manually.
    #if defined(__cplusplus)
    extern "C" {
    #endif
        typedef unsigned __int64 ULONGLONG;
        typedef ULONGLONG(WINAPI* GetTickCount64_t)(void);
        static ULONGLONG WINAPI GetTickCount64(void)
        {
            // Fallback implementation for pre-Vista systems
            static DWORD lastTick = 0;
            static ULONGLONG tick64 = 0;
            DWORD now = GetTickCount();
            if (now < lastTick)
                tick64 += (1ULL << 32); // handle wrap
            lastTick = now;
            return (tick64 & 0xFFFFFFFF00000000ULL) | now;
        }
    #if defined(__cplusplus)
    }
    #endif
#endif

static constexpr ULONG_PTR WCDX_INJECT_TAG = 0x57434458; // 'WCDX' Inject Tag for identifying injected code in the target process.

struct Window_EnumData { const wchar_t* expected; DWORD pid; bool found; };

inline BOOL CALLBACK Title_Check_EnumProc(HWND hwnd, LPARAM lparam)
{
    Window_EnumData* d = reinterpret_cast<Window_EnumData*>(lparam);
    DWORD wpid = 0;
    GetWindowThreadProcessId(hwnd, &wpid);
    if (wpid != d->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;

    wchar_t buf[512] = {};
    int len = GetWindowTextW(hwnd, buf, (int)std::size(buf));
    if (len > 0) {
        if (wcsstr(buf, d->expected) != nullptr) {
            d->found = true;
            return FALSE;
        }
    }
    return TRUE;
}

inline bool Process_Has_Title(const wchar_t* expectedSubstring)
{
    if (!expectedSubstring) return true;
    Window_EnumData data;
    data.expected = expectedSubstring;
    data.pid = GetCurrentProcessId();
    data.found = false;
    EnumWindows(Title_Check_EnumProc, (LPARAM)&data);
    return data.found;
}

//*******************************************Pull_Byte********************************************
// Enables the ability to read bytes from the target process while it is running.
//
// Usage:
// Pull_Byte(Size_of_Patch, Address);   - returns vector of bytes
//
//*************************************************************************************************

#include <windows.h>
#include <cstdint>
#include <vector>

inline const std::vector<uint8_t> Pull_Byte(const SIZE_T Size_of_Patch, const SIZE_T Address)
{
    std::vector<uint8_t> bytes;
    bytes.resize(Size_of_Patch);

    HMODULE exe = ::GetModuleHandleW(nullptr);
    if (exe == nullptr)
        return bytes;

    auto base = reinterpret_cast<uint8_t*>(exe);
    uint8_t* addr = base + Address;

    ::memcpy(bytes.data(), addr, Size_of_Patch);
    return bytes;
}

//*******************************************Push_Byte********************************************
// Enables the ability to patch bytes into the target process while it is running.
// Can change data values on the fly or null out code by replacing with NOPs. (0x90)
//
// Usage:
// Push_Byte(Size_of_Patch, Address, {0x90,0x90,...});
//
//*************************************************************************************************

inline bool Push_Byte(const SIZE_T Size_of_Patch, const SIZE_T Address, const std::vector<uint8_t>& New_Bytes)
{
    if (New_Bytes.size() < Size_of_Patch) return false;
    const uint8_t* patch = New_Bytes.data();

    HMODULE exe = ::GetModuleHandleW(nullptr);
    if (exe == nullptr)
        return false;

    auto base = reinterpret_cast<uint8_t*>(exe);
    uint8_t* addr = base + Address;

    DWORD oldProtect = 0;
    if (!::VirtualProtect(addr, Size_of_Patch, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    if (patch != nullptr)
    {
        ::memcpy(addr, patch, Size_of_Patch);
    }
    else
    {
        for (SIZE_T i = 0; i < Size_of_Patch; ++i)
            addr[i] = 0x90; // NOP
    }

    ::VirtualProtect(addr, Size_of_Patch, oldProtect, &oldProtect);
    return true;
}

//*****************************************Bytes_To_String*****************************************
// Converts a std::vector<uint8_t> to a std::string.
// The resulting string will have the same length as the vector and may contain null bytes.
//
// Usage:
// std::string s = Bytes_To_String(vec);
//
//*************************************************************************************************

#include <string>

inline std::string Bytes_To_String(const std::vector<uint8_t>& vec) {
    return std::string(reinterpret_cast<const char*>(vec.data()), vec.size());
}
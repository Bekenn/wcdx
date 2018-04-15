#include "platform.h"

#include "windows_error.h"

#include <locale>   // for wstring_convert
#include <new>  // for bad_alloc


static std::string convert(const std::wstring& str);
static std::wstring convert(const std::string& str);

const windows_error_category& windows_category() _NOEXCEPT
{
    static windows_error_category category;
    return category;
}

std::string windows_error_category::message(int ev) const
{
    LPSTR allocatedMessage;
    DWORD length = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, ev, 0,
        reinterpret_cast<LPSTR>(&allocatedMessage), 0, nullptr);

    if (length == 0)
        throw std::bad_alloc();

    ::LocalFree(allocatedMessage);

    std::string message(length + 1, L'\0');
    length = ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, ev, 0, &message.front(), length, nullptr);
    if (length == 0)
        message = "Failed to retrieve system error message.";
    else
        message.resize(message.size() - 1); // remove extra NUL terminator from FormatMessage

    return message;
}

std::wstring windows_error_category::wmessage(int ev) const
{
    LPWSTR allocatedMessage;
    DWORD length = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, ev, 0,
        reinterpret_cast<LPWSTR>(&allocatedMessage), 0, nullptr);

    if (length == 0)
        throw std::bad_alloc();

    ::LocalFree(allocatedMessage);

    std::wstring message(length + 1, L'\0');
    length = ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, ev, 0, &message.front(), length, nullptr);
    if (length == 0)
        message = L"Failed to retrieve system error message.";
    else
        message.resize(message.size() - 1); // remove extra NUL terminator from FormatMessage

    return message;
}

windows_error::windows_error(int ev, const std::string& what_arg) : system_error(ev, windows_category(), what_arg),
    what_arg(convert(what_arg))
{
}

windows_error::windows_error(int ev, const std::wstring& what_arg) : system_error(ev, windows_category(), convert(what_arg)),
    what_arg(what_arg)
{
}

windows_error::windows_error(int ev, const char* what_arg) : system_error(ev, windows_category(), what_arg),
    what_arg(convert(what_arg))
{
}

windows_error::windows_error(int ev, const wchar_t* what_arg) : system_error(ev, windows_category(), convert(what_arg)),
    what_arg(what_arg)
{
}

windows_error::windows_error(int ev) : system_error(ev, windows_category())
{
}

windows_error::windows_error() : system_error(::GetLastError(), windows_category())
{
}


std::string convert(const std::wstring& str)
{
    int length = ::WideCharToMultiByte(CP_ACP, 0, str.data(), str.size(), nullptr, 0, nullptr, nullptr);
    if (length == 0)
        throw windows_error(GetLastError(), "Error converting exception context message from Unicode to ANSI");

    std::string converted(length, '\0');
    length = ::WideCharToMultiByte(CP_ACP, 0, str.data(), str.size(), &converted.front(), length, nullptr, nullptr);
    return converted;
}

std::wstring convert(const std::string& str)
{
    int length = ::MultiByteToWideChar(CP_ACP, 0, str.data(), str.size(), nullptr, 0);
    if (length == 0)
        throw windows_error(GetLastError(), "Error converting exception context message from ANSI to Unicode");

    std::wstring converted(length, L'\0');
    length = ::MultiByteToWideChar(CP_ACP, 0, str.data(), str.size(), &converted.front(), length);
    return converted;
}

#ifndef WINDOWS_WINDOWSERROR_INCLUDED
#define WINDOWS_WINDOWSERROR_INCLUDED

#include <system_error>

class windows_error_category : public std::error_category
{
public:
    virtual ~windows_error_category() noexcept { };

public:
    const char* name() const noexcept override { return "windows"; }
    std::string message(int ev) const override;
    std::wstring wmessage(int ev) const;
};

const windows_error_category& windows_category() noexcept;


class windows_error : public std::system_error
{
public:
    windows_error(int ev, const std::string& what_arg);
    windows_error(int ev, const std::wstring& what_arg);
    windows_error(int ev, const char* what_arg);
    windows_error(int ev, const wchar_t* what_arg);
    explicit windows_error(int ev);
    windows_error();

public:
    const wchar_t* wwhat() const noexcept { return what_arg.c_str(); }

private:
    std::wstring what_arg;
};

#endif

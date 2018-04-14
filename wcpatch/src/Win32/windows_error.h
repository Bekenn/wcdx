#ifndef WINDOWS_WINDOWSERROR_INCLUDED
#define WINDOWS_WINDOWSERROR_INCLUDED

#include <system_error>

namespace windows
{
    class windows_error_category : public std::error_category
    {
    public:
        virtual ~windows_error_category() _NOEXCEPT { };

    public:
        const char* name() const _NOEXCEPT override { return "windows"; }
        std::string message(int ev) const override;
        std::wstring wmessage(int ev) const;
    };

    const windows_error_category& windows_category() _NOEXCEPT;


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
        const wchar_t* wwhat() const _NOEXCEPT { return what_arg.c_str(); }

    private:
        std::wstring what_arg;
    };
}

#endif

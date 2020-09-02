#include "platform.h"

#include "resource_stream.h"

#include "windows_error.h"

#include <map>
#include <optional>

#include <cassert>


namespace
{
    struct resource
    {
        const std::byte* data;
        size_t size;
    };

    std::optional<resource> load_resource(uint32_t id);
}

struct resource_stream::impl
{
    resource res;

    impl() = default;
    explicit impl(uint32_t id)
    {
        auto _res = load_resource(id);
        if (_res == std::nullopt)
            throw windows_error();
        res = *_res;
    }
};

resource_stream::resource_stream() noexcept = default;

resource_stream::resource_stream(std::unique_ptr<impl> pimpl) : memory_input_stream(pimpl->res.data, pimpl->res.size)
    , pimpl(std::move(pimpl))
{
}

resource_stream::resource_stream(uint32_t id) : resource_stream(std::make_unique<impl>(id))
{
}

resource_stream::resource_stream(resource_stream&&) noexcept = default;
resource_stream& resource_stream::operator = (resource_stream&& other) noexcept = default;

resource_stream::~resource_stream() = default;

std::error_code resource_stream::open(uint32_t id)
{
    assert(!is_open());

    auto res = load_resource(id);
    if (res == std::nullopt)
        return { int(::GetLastError()), std::system_category() };

    pimpl = std::make_unique<impl>();
    pimpl->res = *res;
    reset(res->data, res->size);

    return { };
}

bool resource_stream::is_open() const noexcept
{
    return pimpl != nullptr;
}

void resource_stream::close() noexcept
{
    pimpl = nullptr;
    reset();
}

namespace
{
    std::optional<resource> load_resource(uint32_t id)
    {
        HMODULE module;
        if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, L"Nobody expects the Spanish Inquisition!", &module))
            return std::nullopt;
        HRSRC hres = ::FindResource(module, MAKEINTRESOURCE(id), RT_RCDATA);
        if (hres == nullptr)
            return std::nullopt;
        HGLOBAL hdata = ::LoadResource(module, hres);
        if (hdata == nullptr)
            return std::nullopt;
        DWORD size = ::SizeofResource(module, hres);
        if (size == 0 && ::GetLastError() != ERROR_SUCCESS)
            return std::nullopt;
        void* data = ::LockResource(hdata);
        if (data == nullptr)
            return std::nullopt;

        return resource{ static_cast<const std::byte*>(data), size };
    }
}

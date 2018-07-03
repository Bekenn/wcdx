#ifndef RESOURCE_INCLUDED
#define RESOURCE_INCLUDED
#pragma once

#include <stdext/traits.h>

#include <functional>


template <class Handle>
class SmartResource
{
public:
    using Deleter = std::function<void (Handle)>;

public:
    template <class H = Handle, REQUIRES(std::is_default_constructible_v<H>)>
    SmartResource()
        : handle(), deleter()
    {
    }

    template <class H,
        REQUIRES(std::is_constructible_v<Handle, decltype(std::forward<H>(std::declval<H&&>()))>)>
    explicit SmartResource(H&& handle, Deleter deleter)
        : handle(std::forward<H>(handle)), deleter(std::move(deleter))
    {
    }

    SmartResource(const SmartResource&) = delete;
    SmartResource& operator = (const SmartResource&) = delete;

    SmartResource(SmartResource&& other)
        : handle(std::move(other.handle)), deleter(std::move(other.deleter))
    {
    }

    SmartResource& operator = (SmartResource&& other)
    {
        if (deleter != nullptr)
            deleter(handle);

        handle = std::move(other.handle);
        deleter = std::move(other.deleter);
    }

    ~SmartResource()
    {
        if (deleter != nullptr)
            deleter(handle);
    }

public:
    operator Handle () const { return handle; }
    Handle Get() const { return handle; }

    template <class H,
        REQUIRES(std::is_assignable_v<Handle&, decltype(std::forward<H>(std::declval<H&&>()))>)>
    void Reset(H&& handle, Deleter deleter)
    {
        this->handle = std::forward<H>(handle);
        this->deleter = std::move(deleter);
    }

    void Invalidate() { deleter = nullptr; }

private:
    Handle handle;
    Deleter deleter;
};

#endif

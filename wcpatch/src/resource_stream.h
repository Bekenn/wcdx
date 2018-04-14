#ifndef RESOURCE_STREAM_INCLUDED
#define RESOURCE_STREAM_INCLUDED
#pragma once

#include <stdext/stream.h>

#include <memory>
#include <cstdint>

class resource_stream : public stdext::memory_input_stream
{
public:
    explicit resource_stream(uint32_t id);
    resource_stream(const resource_stream&) = delete;
    resource_stream& operator = (const resource_stream&) = delete;
    resource_stream(resource_stream&&) = default;
    resource_stream& operator = (resource_stream&& other) = default;
    ~resource_stream() override;

private:
    struct impl;
    explicit resource_stream(std::unique_ptr<impl> pimpl);

private:
    std::unique_ptr<impl> pimpl;
};

#endif

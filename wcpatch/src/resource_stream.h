#ifndef RESOURCE_STREAM_INCLUDED
#define RESOURCE_STREAM_INCLUDED
#pragma once

#include <iolib/stream.h>
#include <memory>
#include <stdint.h>

class resource_stream : public iolib::input_stream
{
public:
	explicit resource_stream(uint32_t id);
	~resource_stream() override;

private:
	struct impl;
	std::unique_ptr<impl> pimpl;
};

#endif

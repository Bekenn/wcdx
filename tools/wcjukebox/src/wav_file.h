#pragma once

#include <stdext/multi.h>
#include <stdext/stream.h>

#include <cstdint>


using seekable_output_stream_ref = stdext::multi_ref<stdext::output_stream, stdext::seekable>;

void write_wave(seekable_output_stream_ref out, stdext::input_stream& in, uint16_t channels, uint32_t sample_rate, uint16_t bits_per_sample, size_t buffer_size);

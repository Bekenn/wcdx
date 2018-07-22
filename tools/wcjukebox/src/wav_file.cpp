#include "wav_file.h"

#include <stdext/endian.h>

#include <algorithm>
#include <memory>


using namespace stdext::literals;

namespace
{
    enum class wave_format : uint16_t
    {
        unknown = 0x0000,
        pcm     = 0x0001,
        adpcm   = 0x0002,
        alaw    = 0x0006,
        mulaw   = 0x0007,
        gsm610  = 0x0031,
        mpeg    = 0x0050
    };

    class riff_chunk_writer
    {
    public:
        explicit riff_chunk_writer(seekable_output_stream_ref out, uint32_t chunk_id) noexcept;
        riff_chunk_writer(const riff_chunk_writer&) = delete;
        riff_chunk_writer& operator = (const riff_chunk_writer&) = delete;
        ~riff_chunk_writer();

    private:
        stdext::multi_ref<stdext::output_stream, stdext::seekable> _out;
        stdext::stream_position _size_position;
    };
}

void write_wave(seekable_output_stream_ref out, stdext::input_stream& in, uint16_t channels, uint32_t sample_rate, uint16_t bits_per_sample, size_t buffer_size)
{
    auto& stream = out.as<stdext::output_stream>();

    riff_chunk_writer riff_chunk(out, "RIFF"_4cc);
    stream.write("WAVE"_4cc);

    {
        auto bytes_per_sample = (bits_per_sample + 7) / 8;
        riff_chunk_writer format_chunk(out, "fmt "_4cc);
        stream.write(wave_format::pcm);
        stream.write(channels);
        stream.write(sample_rate);
        stream.write(uint32_t(channels * sample_rate * bytes_per_sample));
        stream.write(uint16_t(channels * bytes_per_sample));
        stream.write(bits_per_sample);
    }

    {
        riff_chunk_writer data_chunk(out, "data"_4cc);

        if (auto direct_writer = dynamic_cast<stdext::direct_writable*>(&stream); direct_writer != nullptr)
        {
            size_t bytes;
            do
            {
                bytes = direct_writer->direct_write([&in](uint8_t* buffer, size_t size)
                {
                    return in.read(buffer, size);
                });
            } while (bytes != 0);
        }
        else if (auto direct_reader = dynamic_cast<stdext::direct_readable*>(&in); direct_reader != nullptr)
        {
            size_t bytes;
            do
            {
                bytes = direct_reader->direct_read([&stream](const uint8_t* buffer, size_t size)
                {
                    return stream.write(buffer, size);
                });
            } while (bytes != 0);
        }
        else
        {
            buffer_size = std::max(buffer_size, size_t(0x1000));
            auto buffer = std::make_unique<uint8_t[]>(buffer_size);

            size_t bytes;
            do
            {
                bytes = in.read(buffer.get(), buffer_size);
                stream.write(buffer.get(), bytes);
            } while (bytes != 0);
        }
    }
}

namespace
{
    riff_chunk_writer::riff_chunk_writer(seekable_output_stream_ref out, uint32_t chunk_id) noexcept
        : _out(out)
    {
        auto& stream = _out.as<stdext::output_stream>();
        stream.write(chunk_id);
        _size_position = _out.as<stdext::seekable>().position();
        stream.write(uint32_t(0));
    }

    riff_chunk_writer::~riff_chunk_writer()
    {
        auto& seeker = _out.as<stdext::seekable>();
        auto position = seeker.position();
        seeker.set_position(_size_position);
        _out.as<stdext::output_stream>().write(uint32_t(position - (_size_position + sizeof(uint32_t))));
        seeker.set_position(position);
    }
}
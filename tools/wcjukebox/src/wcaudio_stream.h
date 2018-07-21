#pragma once

#include <stdext/array_view.h>
#include <stdext/multi.h>
#include <stdext/stream.h>

#include <vector>

#include <cstddef>
#include <cstdint>


constexpr auto end_of_track = uint32_t(-1);
constexpr auto no_trigger = uint8_t(-1);

struct stream_file_header
{
    uint32_t magic;
    uint32_t version;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint16_t sample_rate;
    uint32_t buffer_size;
    uint32_t reserved1;
    uint32_t chunk_headers_offset;
    uint32_t chunk_count;
    uint32_t chunk_link_offset;
    uint32_t chunk_link_count;
    uint32_t trigger_link_offset;
    uint32_t trigger_link_count;
    uint32_t file_buffer_size;
    uint32_t thing4_offset;
    uint32_t thing4_count;
    uint32_t thing5_offset;
    uint32_t thing5_count;
    uint32_t thing6_offset;
    uint32_t thing6_count;
    uint8_t reserved2[32];
};

struct chunk_header
{
    uint32_t start_offset;
    uint32_t end_offset;
    uint32_t trigger_link_count;
    uint32_t trigger_link_index;
    uint32_t chunk_link_count;
    uint32_t chunk_link_index;
};

#pragma pack(push)
#pragma pack(1)
struct stream_chunk_link
{
    uint8_t intensity;
    uint32_t chunk_index;
};

struct stream_trigger_link
{
    uint8_t trigger;
    uint32_t chunk_index;
};
#pragma pack(pop)

class wcaudio_stream : public stdext::input_stream
{
public:
    explicit wcaudio_stream(stdext::multi_ref<stdext::input_stream, stdext::seekable> stream);

public:
    void select(uint8_t trigger, uint8_t intensity);

    const stream_file_header& file_header() const
    {
        return _file_header;
    }

    stdext::const_array_view<chunk_header> chunks() const
    {
        return { _chunks.data(), _chunks.size() };
    }

    stdext::const_array_view<stream_chunk_link> chunk_links() const
    {
        return { _chunk_links.data(), _chunk_links.size() };
    }

    stdext::const_array_view<stream_trigger_link> trigger_links() const
    {
        return { _trigger_links.data(), _trigger_links.size() };
    }

private:
    virtual size_t do_read(uint8_t* buffer, size_t size);
    virtual size_t do_skip(size_t size);

    uint32_t next_chunk_index(uint32_t chunk_index, uint8_t trigger, uint8_t intensity);

private:
    stdext::multi_ptr<stdext::input_stream, stdext::seekable> _stream;
    stream_file_header _file_header;

    std::vector<chunk_header> _chunks;
    std::vector<stream_chunk_link> _chunk_links;
    std::vector<stream_trigger_link> _trigger_links;

    chunk_header* _current_chunk;
    uint32_t _current_chunk_offset;
    uint8_t _current_intensity;
};

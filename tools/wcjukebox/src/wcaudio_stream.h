#pragma once

#include <stdext/array_view.h>
#include <stdext/multi.h>
#include <stdext/stream.h>

#include <functional>
#include <utility>
#include <vector>

#include <cstddef>
#include <cstdint>


constexpr auto end_of_track = uint32_t(-1);
constexpr auto no_trigger = uint8_t(-1);

struct chunk_header;
struct stream_chunk_link;
struct stream_trigger_link;

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

class wcaudio_stream : public stdext::input_stream
{
public:
    using next_chunk_handler = std::function<void (uint32_t chunk_index, unsigned frame_count)>;
    using loop_handler = std::function<bool (uint32_t chunk_index, unsigned frame_count)>;
    using start_track_handler = std::function<void (uint32_t chunk_index)>;
    using next_track_handler = std::function<bool (uint32_t chunk_index, unsigned frame_count)>;
    using prev_track_handler = std::function<void (unsigned frame_count)>;
    using end_of_stream_handler = std::function<void (unsigned frame_count)>;

public:
    explicit wcaudio_stream(stdext::multi_ref<stdext::input_stream, stdext::seekable> stream);
    ~wcaudio_stream() override;

public:
    uint8_t channels() const;
    uint8_t bits_per_sample() const;
    uint16_t sample_rate() const;
    uint32_t buffer_size() const;

    std::vector<uint8_t> triggers() const;
    std::vector<uint8_t> intensities() const;

    void select(uint8_t trigger, uint8_t intensity);

    void on_next_chunk(next_chunk_handler handler);
    void on_loop(loop_handler handler);
    void on_start_track(start_track_handler handler);
    void on_next_track(next_track_handler handler);
    void on_prev_track(prev_track_handler handler);
    void on_end_of_stream(end_of_stream_handler handler);

private:
    size_t do_read(uint8_t* buffer, size_t size) override;
    size_t do_skip(size_t size) override;

    uint32_t next_chunk_index(uint32_t chunk_index, uint8_t trigger, uint8_t intensity);

private:
    stdext::multi_ptr<stdext::input_stream, stdext::seekable> _stream;
    stream_file_header _file_header;

    std::vector<chunk_header> _chunks;
    std::vector<stream_chunk_link> _chunk_links;
    std::vector<stream_trigger_link> _trigger_links;

    next_chunk_handler _next_chunk_handler;
    loop_handler _loop_handler;
    start_track_handler _start_track_handler;
    next_track_handler _next_track_handler;
    prev_track_handler _prev_track_handler;
    end_of_stream_handler _end_of_stream_handler;

    chunk_header* _current_chunk = nullptr;
    uint32_t _current_chunk_offset = 0;
    uint8_t _current_intensity = 0;

    unsigned _frame_count = 0;
    uint32_t _first_chunk_index = 0;
};

inline uint8_t wcaudio_stream::channels() const
{
    return _file_header.channels;
}

inline uint8_t wcaudio_stream::bits_per_sample() const
{
    return _file_header.bits_per_sample;
}

inline uint16_t wcaudio_stream::sample_rate() const
{
    return _file_header.sample_rate;
}

inline uint32_t wcaudio_stream::buffer_size() const
{
    return _file_header.buffer_size;
}

inline void wcaudio_stream::on_next_chunk(next_chunk_handler handler)
{
    _next_chunk_handler = std::move(handler);
}

inline void wcaudio_stream::on_loop(loop_handler handler)
{
    _loop_handler = std::move(handler);
}

inline void wcaudio_stream::on_start_track(start_track_handler handler)
{
    _start_track_handler = std::move(handler);
}

inline void wcaudio_stream::on_next_track(next_track_handler handler)
{
    _next_track_handler = std::move(handler);
}

inline void wcaudio_stream::on_prev_track(prev_track_handler handler)
{
    _prev_track_handler = std::move(handler);
}

inline void wcaudio_stream::on_end_of_stream(end_of_stream_handler handler)
{
    _end_of_stream_handler = std::move(handler);
}

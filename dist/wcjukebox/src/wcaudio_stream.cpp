#include "wcaudio_stream.h"

#include <stdext/endian.h>

#include <algorithm>


using namespace stdext::literals;

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

wcaudio_stream::wcaudio_stream(stdext::multi_ref<stdext::input_stream, stdext::seekable> stream)
    : _stream(&stream)
{
    auto& in = stream.as<stdext::input_stream>();
    auto& seeker = stream.as<stdext::seekable>();

    _file_header = in.read<stream_file_header>();
    if (_file_header.magic != "STRM"_4cc)
        throw std::runtime_error("Invalid stream.");

    _chunks.resize(_file_header.chunk_count);
    seeker.seek(stdext::seek_from::begin, _file_header.chunk_headers_offset);
    in.read_all(_chunks.data(), _chunks.size());

    _chunk_links.resize(_file_header.chunk_link_count);
    seeker.seek(stdext::seek_from::begin, _file_header.chunk_link_offset);
    in.read_all(_chunk_links.data(), _chunk_links.size());

    _trigger_links.resize(_file_header.trigger_link_count);
    seeker.seek(stdext::seek_from::begin, _file_header.trigger_link_offset);
    in.read_all(_trigger_links.data(), _trigger_links.size());
}

wcaudio_stream::~wcaudio_stream() = default;

std::vector<uint8_t> wcaudio_stream::triggers() const
{
    auto& chunk = _chunks[0];
    stdext::array_view<const stream_trigger_link> trigger_links(&_trigger_links[chunk.trigger_link_index], chunk.trigger_link_count);
    std::vector<uint8_t> triggers;
    triggers.reserve(trigger_links.size());
    for (auto& link : trigger_links)
        triggers.push_back(link.trigger);
    return triggers;
}

std::vector<uint8_t> wcaudio_stream::intensities() const
{
    auto& index_chunk = _chunks[0];
    stdext::array_view<const stream_chunk_link> chunk_links(&_chunk_links[index_chunk.chunk_link_index], index_chunk.chunk_link_count);
    std::vector<uint8_t> intensities;
    intensities.reserve(chunk_links.size());
    for (auto& link : chunk_links)
        intensities.push_back(link.intensity);
    return intensities;
}

void wcaudio_stream::select(uint8_t trigger, uint8_t intensity)
{
    auto chunk_index = next_chunk_index(0, trigger, intensity);
    if (chunk_index == end_of_track)
        return;

    _current_chunk = &_chunks[chunk_index];
    _current_chunk_offset = 0;
    _current_intensity = intensity;

    _frame_count = 0;
}

size_t wcaudio_stream::do_read(std::byte* buffer, size_t size)
{
    if (_current_chunk == nullptr)
        return 0;

    auto& in = *_stream.as<stdext::input_stream>();
    auto& seeker = *_stream.as<stdext::seekable>();
    size_t total_bytes = 0;
    auto p = buffer;
    while (size != 0)
    {
        seeker.set_position(stdext::stream_position(_current_chunk->start_offset) + _current_chunk_offset);
        auto chunk_size = _current_chunk->end_offset - _current_chunk->start_offset;
        auto bytes = std::min(size_t(chunk_size - _current_chunk_offset), size);

        if (buffer == nullptr)
            bytes = in.skip<std::byte>(bytes);
        else
        {
            bytes = in.read(p, bytes);
            p += bytes;
        }

        size -= bytes;
        total_bytes += bytes;
        _current_chunk_offset += uint32_t(bytes);
        if (_current_chunk_offset == chunk_size)
        {
            _frame_count += chunk_size / (_file_header.channels * ((_file_header.bits_per_sample + 7) / 8));
            _current_chunk_offset = 0;

            auto index = next_chunk_index(uint32_t(_current_chunk - &_chunks.front()), no_trigger, _current_intensity);
            if (index == end_of_track)
            {
                _current_chunk = nullptr;
                break;
            }

            _current_chunk = &_chunks[index];
        }
    }

    return total_bytes;
}

size_t wcaudio_stream::do_skip(size_t size)
{
    return do_read(nullptr, size);
}

uint32_t wcaudio_stream::next_chunk_index(uint32_t chunk_index, uint8_t trigger, uint8_t intensity)
{
    const auto& chunk = _chunks[chunk_index];
    auto track_link_first = begin(_trigger_links) + chunk.trigger_link_index;
    auto track_link_last = track_link_first + chunk.trigger_link_count;
    for (; track_link_first != track_link_last; ++track_link_first)
    {
        switch (track_link_first->trigger)
        {
        case 64:
            if (_end_of_stream_handler != nullptr)
                _end_of_stream_handler(_frame_count);
            return end_of_track;
        case 65:
            if (_prev_track_handler != nullptr)
                _prev_track_handler(_frame_count);
            return end_of_track;
        default:
            if (track_link_first->trigger == trigger)
            {
                if (_start_track_handler != nullptr)
                    _start_track_handler(track_link_first->chunk_index);
                _first_chunk_index = track_link_first->chunk_index;
                return track_link_first->chunk_index;
            }
            break;
        }
    }

    auto chunk_link_first = begin(_chunk_links) + chunk.chunk_link_index;
    auto chunk_link_last = chunk_link_first + chunk.chunk_link_count;
    auto closest_intensity_level = 256;
    auto closest_intensity_index = -1;
    for (; chunk_link_first != chunk_link_last; ++chunk_link_first)
    {
        auto delta = abs(chunk_link_first->intensity - intensity);
        if (delta < closest_intensity_level)
        {
            closest_intensity_level = delta;
            closest_intensity_index = chunk_link_first->chunk_index;
        }
    }

    if (closest_intensity_index != -1)
    {
        auto current_chunk_index = _current_chunk - _chunks.data();
        if (closest_intensity_index == current_chunk_index + 1)
        {
            if (_next_chunk_handler != nullptr)
                _next_chunk_handler(closest_intensity_index, _frame_count);
        }
        else if (closest_intensity_index < current_chunk_index
                 && uint32_t(closest_intensity_index) >= _first_chunk_index)
        {
            if (_loop_handler != nullptr && !_loop_handler(closest_intensity_index, _frame_count))
                return end_of_track;
        }
        else
        {
            if (_next_track_handler != nullptr && !_next_track_handler(closest_intensity_index, _frame_count))
                return end_of_track;
            _first_chunk_index = closest_intensity_index;
        }

        return closest_intensity_index;
    }

    if (++chunk_index == _chunks.size())
    {
        chunk_index = 0;
        _first_chunk_index = 0;
        if (_next_track_handler != nullptr && !_next_track_handler(chunk_index, _frame_count))
            return end_of_track;
    }
    else if (_next_chunk_handler != nullptr)
        _next_chunk_handler(chunk_index, _frame_count);

    return chunk_index;
}

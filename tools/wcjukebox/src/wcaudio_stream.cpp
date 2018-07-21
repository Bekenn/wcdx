#include "wcaudio_stream.h"

#include <algorithm>
#include <iostream>


wcaudio_stream::wcaudio_stream(stdext::multi_ref<stdext::input_stream, stdext::seekable> stream)
    : _stream(&stream)
{
    auto& in = stream.as<stdext::input_stream>();
    auto& seeker = stream.as<stdext::seekable>();

    _file_header = in.read<stream_file_header>();
    if (_file_header.magic != 'MRTS')
        throw std::runtime_error("Invalid stream.");

    _chunks.resize(_file_header.chunk_count);
    seeker.seek(stdext::seek_from::begin, _file_header.chunk_headers_offset);
    in.read(_chunks.data(), _chunks.size());

    _chunk_links.resize(_file_header.chunk_link_count);
    seeker.seek(stdext::seek_from::begin, _file_header.chunk_link_offset);
    in.read(_chunk_links.data(), _chunk_links.size());

    _trigger_links.resize(_file_header.trigger_link_count);
    seeker.seek(stdext::seek_from::begin, _file_header.trigger_link_offset);
    in.read(_trigger_links.data(), _trigger_links.size());
}

void wcaudio_stream::select(uint8_t trigger, uint8_t intensity)
{
    auto index = next_chunk_index(0, trigger, intensity);
    if (index == end_of_track)
        return;

    _current_chunk = &_chunks[index];
    _current_chunk_offset = 0;
    _current_intensity = intensity;
}

size_t wcaudio_stream::do_read(uint8_t* buffer, size_t size)
{
    if (_current_chunk == nullptr)
        return 0;

    auto& in = *_stream.as<stdext::input_stream>();
    auto& seeker = *_stream.as<stdext::seekable>();
    size_t total_bytes = 0;
    auto p = buffer;
    while (size != 0)
    {
        seeker.seek(stdext::seek_from::begin, _current_chunk->start_offset + _current_chunk_offset);
        auto chunk_size = _current_chunk->end_offset - _current_chunk->start_offset;
        auto bytes = std::min(chunk_size - _current_chunk_offset, size);

        if (buffer == nullptr)
            bytes = in.skip<uint8_t>(bytes);
        else
        {
            bytes = in.read(p, bytes);
            p += bytes;
        }

        size -= bytes;
        total_bytes += bytes;
        _current_chunk_offset += bytes;
        if (_current_chunk_offset == chunk_size)
        {
            _current_chunk_offset = 0;

            auto index = next_chunk_index(_current_chunk - &_chunks.front(), no_trigger, _current_intensity);
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
            std::cout << "End of stream." << std::endl;
            return end_of_track;
        case 65:
            std::cout << "Go to prior trigger." << std::endl;
            return end_of_track;
        default:
            if (track_link_first->trigger == trigger)
                return track_link_first->chunk_index;
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
        return closest_intensity_index;

    if (++chunk_index == _chunks.size())
    {
        std::cout << "Looping to start of file.\n";
        chunk_index = 0;
    }

    return chunk_index;
}

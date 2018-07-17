#include "dsound_player.h"
#include "dsound_error.h"

#include <iostream>

#include <InitGuid.h>
DEFINE_GUID(DSDEVID_DefaultPlayback, 0xdef00000, 0x9c6d, 0x47ed, 0xaa, 0xf1, 0x4d, 0xda, 0x8f, 0x2b, 0x5c, 0x03);
DEFINE_GUID(IID_IDirectSoundBuffer8, 0x6825a449, 0x7524, 0x4d82, 0x92, 0x0f, 0x50, 0xe3, 0x6a, 0xb3, 0xab, 0x1e);
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);


dsound_player::dsound_player()
{
    // DirectSound needs a window handle for SetCooperativeLevel (nullptr doesn't work).
    auto window = ::CreateWindow(L"BUTTON", L"Hidden DirectSound Window", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);
    auto hr = ::DirectSoundCreate8(&DSDEVID_DefaultPlayback, &_ds8, nullptr);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    hr = _ds8->SetCooperativeLevel(window, DSSCL_PRIORITY);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    DSBUFFERDESC desc =
    {
        sizeof(desc),
        DSBCAPS_PRIMARYBUFFER,
        0, 0, nullptr,
        DS3DALG_DEFAULT
    };

    hr = _ds8->CreateSoundBuffer(&desc, &_dsbuffer_primary, nullptr);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());
}

void dsound_player::load(stdext::multi_ref<stdext::input_stream, stdext::seekable> stream)
{
    _stream = &stream;
    auto& in = stream.as<stdext::input_stream>();
    auto& seeker = stream.as<stdext::seekable>();

    _file_header = in.read<stream_file_header>();

    _chunks.resize(_file_header.chunk_count);
    seeker.seek(stdext::seek_from::begin, _file_header.chunk_headers_offset);
    in.read(_chunks.data(), _chunks.size());

    _chunk_links.resize(_file_header.chunk_link_count);
    seeker.seek(stdext::seek_from::begin, _file_header.chunk_link_offset);
    in.read(_chunk_links.data(), _chunk_links.size());

    _track_links.resize(_file_header.trigger_link_count);
    seeker.seek(stdext::seek_from::begin, _file_header.trigger_link_offset);
    in.read(_track_links.data(), _track_links.size());

    PCMWAVEFORMAT format =
    {
        WAVE_FORMAT_PCM,
        _file_header.channels,
        _file_header.sample_rate,
        DWORD(_file_header.sample_rate * _file_header.channels * (_file_header.bits_per_sample / 8)),
        WORD(_file_header.channels * (_file_header.bits_per_sample / 8)),
        _file_header.bits_per_sample
    };

    HRESULT hr = _dsbuffer_primary->SetFormat(reinterpret_cast<LPWAVEFORMATEX>(&format));
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    DSBUFFERDESC desc =
    {
        sizeof(desc),
        DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY,
        _file_header.buffer_size,
        0,
        reinterpret_cast<WAVEFORMATEX*>(&format),
        DS3DALG_DEFAULT
    };

    IDirectSoundBufferPtr dsbuffer;
    hr = _ds8->CreateSoundBuffer(&desc, &dsbuffer, nullptr);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    _dsbuffer8 = std::move(dsbuffer);
    if (_dsbuffer8 == nullptr)
        _com_raise_error(E_NOINTERFACE);

    auto notify = IDirectSoundNotify8Ptr(_dsbuffer8);
    if (notify == nullptr)
        _com_raise_error(E_NOINTERFACE);

    _position_event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (_position_event == nullptr)
        throw std::system_error(::GetLastError(), std::system_category());

    DSBPOSITIONNOTIFY positions[] =
    {
        { 0, _position_event },
        { _file_header.buffer_size / 2, _position_event },
    };
    hr = notify->SetNotificationPositions(stdext::lengthof(positions), positions);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());
}

void dsound_player::play(uint8_t trigger, uint8_t intensity)
{
    auto index = next_chunk_index(0, trigger, intensity);
    if (index == end_of_track)
        return;

    trigger = no_trigger;

    const chunk_header* chunk = &_chunks[index];
    uint32_t chunk_offset = 0;

    uint8_t* buffer;
    DWORD size;
    auto hr = _dsbuffer8->Lock(0, _file_header.buffer_size, reinterpret_cast<LPVOID*>(&buffer), &size, nullptr, nullptr, 0);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());
    stream_chunks(chunk, chunk_offset, trigger, intensity, buffer, size);
    _dsbuffer8->Unlock(buffer, size, nullptr, 0);

    hr = _dsbuffer8->Play(0, 0, DSBPLAY_LOOPING);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    // Consume one event because we have two full sections in the buffer.
    ::WaitForSingleObject(_position_event, INFINITE);
    ::ResetEvent(_position_event);

    bool done = false;
    while (!done)
    {
        auto wait_result = ::WaitForSingleObject(_position_event, INFINITE);
        if (wait_result == WAIT_FAILED)
            throw std::system_error(::GetLastError(), std::system_category());
        ::ResetEvent(_position_event);

        DWORD play, write;
        _dsbuffer8->GetCurrentPosition(&play, &write);
        auto midpoint = _file_header.buffer_size / 2;
        auto buffer_offset = play < midpoint ? midpoint : 0;
        size = buffer_offset == 0 ? midpoint : _file_header.buffer_size - midpoint;
        _dsbuffer8->Lock(buffer_offset, size, reinterpret_cast<LPVOID*>(&buffer), &size, nullptr, nullptr, 0);
        if (chunk == nullptr)
        {
            std::fill_n(buffer, size, 0);
            done = true;
        }
        else
            stream_chunks(chunk, chunk_offset, trigger, intensity, buffer, size);
        _dsbuffer8->Unlock(buffer, size, nullptr, 0);
    }

    ::WaitForSingleObject(_position_event, INFINITE);
    ::ResetEvent(_position_event);
    _dsbuffer8->Stop();
}

void dsound_player::stream_chunks(const chunk_header*& chunk, uint32_t& chunk_offset, uint8_t trigger, uint8_t intensity, uint8_t* p, size_t length)
{
    auto& in = *_stream.as<stdext::input_stream>();
    auto& seeker = *_stream.as<stdext::seekable>();
    while (length != 0)
    {
        seeker.seek(stdext::seek_from::begin, chunk->start_offset + chunk_offset);
        auto chunk_size = chunk->end_offset - chunk->start_offset;
        auto bytes = std::min(chunk_size - chunk_offset, length);
        bytes = in.read(p, bytes);
        p += bytes;
        length -= bytes;
        chunk_offset += bytes;
        if (chunk_offset == chunk_size)
        {
            chunk_offset = 0;

            auto index = next_chunk_index(chunk - &_chunks.front(), trigger, intensity);
            if (index == end_of_track)
            {
                chunk = nullptr;
                std::fill_n(p, length, 0);
                return;
            }

            chunk = &_chunks[index];
        }
    }
}

uint32_t dsound_player::next_chunk_index(uint32_t chunk_index, uint8_t trigger, uint8_t intensity)
{
    const auto& chunk = _chunks[chunk_index];
    auto track_link_first = begin(_track_links) + chunk.trigger_link_index;
    auto track_link_last = track_link_first + chunk.trigger_link_count;
    for (; track_link_first != track_link_last; ++track_link_first)
    {
        switch (track_link_first->trigger)
        {
        case 64:
            std::cout << "End of stream.\n";
            return end_of_track;
        case 65:
            std::cout << "Go to prior trigger.\n";
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

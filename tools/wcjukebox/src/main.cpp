#include "dsound_error.h"

#include <stdext/file.h>
#include <stdext/multi.h>
#include <stdext/utility.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <cstdlib>
#include <cstddef>

#define NOMINMAX
#define INITGUID
#include <Windows.h>
#include <dsound.h>
#include <cguid.h>
#include <comip.h>
#include <comdef.h>


namespace
{
    enum stream_archive
    {
        preflight, postflight, mission
    };

    struct game_trigger_desc
    {
        stream_archive archive;
        uint8_t trigger;
    };

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

    class usage_error : public std::runtime_error
    {
        using runtime_error::runtime_error;
    };

    _COM_SMARTPTR_TYPEDEF(IDirectSound8, IID_IDirectSound8);
    _COM_SMARTPTR_TYPEDEF(IDirectSoundBuffer, IID_IDirectSoundBuffer);
    _COM_SMARTPTR_TYPEDEF(IDirectSoundBuffer8, IID_IDirectSoundBuffer8);
    _COM_SMARTPTR_TYPEDEF(IDirectSoundNotify8, IID_IDirectSoundNotify8);

    class dsound_player
    {
    public:
        explicit dsound_player();

    public:
        void load(stdext::multi_ref<stdext::input_stream, stdext::seekable> stream);
        void play(uint8_t trigger, uint8_t intensity);

    private:
        DWORD buffer_size_available();
        void stream_chunks(const chunk_header*& chunk, uint32_t& chunk_offset, uint8_t trigger, uint8_t intensity, uint8_t* p, size_t length);
        uint32_t next_chunk_index(uint32_t chunk_index, uint8_t trigger, uint8_t intensity);

    private:
        stdext::multi_ptr<stdext::input_stream, stdext::seekable> _stream;
        stream_file_header _file_header;

        std::vector<chunk_header> _chunks;
        std::vector<stream_chunk_link> _chunk_links;
        std::vector<stream_trigger_link> _track_links;

        IDirectSound8Ptr _ds8;
        IDirectSoundBufferPtr _dsbuffer_primary;
        IDirectSoundBuffer8Ptr _dsbuffer8;
        HANDLE _position_event;
    };

    void show_usage(const wchar_t* invocation);
    std::string to_mbstring(const wchar_t* str);

    constexpr auto end_of_track = uint32_t(-1);
    constexpr auto no_trigger = uint8_t(-1);

    constexpr const wchar_t* stream_filenames[]
    {
        L"STREAMS\\PREFLITE.STR",
        L"STREAMS\\POSFLITE.STR",
        L"STREAMS\\MISSION.STR"
    };

    // Maps from a track number to a stream archive and trigger number
    // See StreamLoadTrack and GetStreamTrack in Wing1.i64
    constexpr game_trigger_desc track_map[] =
    {
        { stream_archive::mission, no_trigger },    // 0 - Combat 1
        { stream_archive::mission, no_trigger },    // 1 - Combat 2
        { stream_archive::mission, no_trigger },    // 2 - Combat 3
        { stream_archive::mission, no_trigger },    // 3 - Combat 4
        { stream_archive::mission, no_trigger },    // 4 - Combat 5
        { stream_archive::mission, no_trigger },    // 5 - Combat 6
        { stream_archive::mission, 6 },             // 6 - Victorious combat
        { stream_archive::mission, 7 },             // 7 - Tragedy
        { stream_archive::mission, 8 },             // 8 - Dire straits
        { stream_archive::mission, 9 },             // 9 - Scratch one fighter
        { stream_archive::mission, 10 },            // 10 - Defeated fleeing enemy
        { stream_archive::mission, 11 },            // 11 - Wingman death
        { stream_archive::mission, no_trigger },    // 12 - Returning defeated
        { stream_archive::mission, no_trigger },    // 13 - Returning successful
        { stream_archive::mission, no_trigger },    // 14 - Returning jubilant
        { stream_archive::mission, no_trigger },    // 15 - Mission 1
        { stream_archive::mission, no_trigger },    // 16 - Mission 2
        { stream_archive::mission, no_trigger },    // 17 - Mission 3
        { stream_archive::mission, no_trigger },    // 18 - Mission 4
        { stream_archive::preflight, no_trigger },  // 19 - OriginFX (actually, fanfare)
        { stream_archive::preflight, 1 },           // 20 - Arcade Mission
        { stream_archive::preflight, 4 },           // 21 - Arcade Victory
        { stream_archive::preflight, 3 },           // 22 - Arcade Death
        { stream_archive::preflight, no_trigger },  // 23 - Fanfare
        { stream_archive::preflight, 5 },           // 24 - Halcyon's Office 1
        { stream_archive::preflight, 6 },           // 25 - Briefing
        { stream_archive::preflight, 7 },           // 26 - Briefing Dismissed
        { stream_archive::mission, 27 },            // 27 - Scramble
        { stream_archive::postflight, no_trigger }, // 28 - Landing
        { stream_archive::postflight, 0, },         // 29 - Damage Assessment
        { stream_archive::preflight, 0 },           // 30 - Rec Room
        { stream_archive::mission, 31 },            // 31 - Eject
        { stream_archive::mission, 32 },            // 32 - Death
        { stream_archive::postflight, 2 },          // 33 - debriefing (successful)
        { stream_archive::postflight, 1 },          // 34 - debriefing (failed)
        { stream_archive::preflight, 2 },           // 35 - barracks
        { stream_archive::postflight, 3 },          // 36 - Halcyon's Office / Briefing 2
        { stream_archive::postflight, 4 },          // 37 - medal (valor?)
        { stream_archive::postflight, 5 },          // 38 - medal (golden sun?)
        { stream_archive::postflight, 7 },          // 39 - another medal
        { stream_archive::postflight, 6 },          // 40 - big medal
    };
}

int wmain(int argc, wchar_t* argv[])
{
    std::wstring invocation = argc > 0 ? std::filesystem::path(argv[0]).filename() : "wcjukebox";

    try
    {
        if (argc == 1)
        {
            show_usage(invocation.c_str());
            return EXIT_SUCCESS;
        }

        if (argc != 2)
            throw usage_error("Too many arguments.");

        dsound_player player;
        wchar_t* end;
        auto track = int(wcstol(argv[1], &end, 10));
        if (*end != '\0')
        {
            std::ostringstream message;
            message << "Unrecognized argument: " << to_mbstring(argv[1]);
            throw usage_error(std::move(message).str());
        }

        if (track < 0 || unsigned(track) >= stdext::lengthof(track_map))
        {
            std::ostringstream message;
            message << "Track must be between 0 and " << stdext::lengthof(track_map) - 1 << '.';
            throw usage_error(std::move(message).str());
        }

        auto stream_name = stream_filenames[track_map[track].archive];
        stdext::file_input_stream stream(stream_name);
        player.load(stream);

        uint8_t trigger = track_map[track].trigger;
        uint8_t intensity = 15;
        if (track_map[track].archive == stream_archive::mission && trigger == no_trigger)
            intensity = uint8_t(track);
        player.play(trigger, intensity);

        return EXIT_SUCCESS;
    }
    catch (const usage_error& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        show_usage(invocation.c_str());
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Unknown error\n";
    }

    return EXIT_FAILURE;
}

namespace
{
    void show_usage(const wchar_t* invocation)
    {
        std::wcout << L"Usage: " << invocation << L" <tracknum>\n";
    }

    std::string to_mbstring(const wchar_t* str)
    {
        std::mbstate_t state = {};
        auto length = std::wcsrtombs(nullptr, &str, 0, &state);
        if (length == size_t(-1))
            throw std::runtime_error("Unicode error");

        std::string result(length, '\0');
        std::wcsrtombs(result.data(), &str, length, &state);
        return result;
    }

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
        {
            std::cerr << "Couldn't set primary buffer output format.\n";
            exit(EXIT_FAILURE);
        }

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
        _dsbuffer8 = std::move(dsbuffer);
        if (FAILED(hr) || _dsbuffer8 == nullptr)
        {
            std::cerr << "Couldn't create sound buffer.\n";
            exit(EXIT_FAILURE);
        }

        auto notify = IDirectSoundNotify8Ptr(_dsbuffer8);
        if (notify == nullptr)
        {
            std::cerr << "Couldn't create Notify interface.\n";
            exit(EXIT_FAILURE);
        }

        _position_event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (_position_event == nullptr)
        {
            std::cerr << "Couldn't create event object.\n";
            exit(EXIT_FAILURE);
        }

        DSBPOSITIONNOTIFY positions[] =
        {
            { 0, _position_event },
            { _file_header.buffer_size / 2, _position_event },
        };
        hr = notify->SetNotificationPositions(stdext::lengthof(positions), positions);
        if (FAILED(hr))
        {
            std::cerr << "Couldn't set notification positions.\n";
            exit(EXIT_FAILURE);
        }
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
        {
            std::cerr << "Couldn't lock buffer.\n";
            exit(EXIT_FAILURE);
        }
        stream_chunks(chunk, chunk_offset, trigger, intensity, buffer, size);
        _dsbuffer8->Unlock(buffer, size, nullptr, 0);

        hr = _dsbuffer8->Play(0, 0, DSBPLAY_LOOPING);
        if (FAILED(hr))
        {
            std::cerr << "Couldn't play stream.\n";
            exit(EXIT_FAILURE);
        }

        // Consume one event because we have two full sections in the buffer.
        ::WaitForSingleObject(_position_event, INFINITE);
        ::ResetEvent(_position_event);

        bool done = false;
        while (!done)
        {
            auto wait_result = ::WaitForSingleObject(_position_event, INFINITE);
            if (wait_result == WAIT_FAILED)
            {
                std::cerr << "Wait failed.\n";
                exit(EXIT_FAILURE);
            }
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

    DWORD dsound_player::buffer_size_available()
    {
        DWORD play_cursor;
        DWORD write_cursor;
        auto hr = _dsbuffer8->GetCurrentPosition(&play_cursor, &write_cursor);
        if (FAILED(hr))
        {
            std::cerr << "Couldn't get buffer size.\n";
            exit(EXIT_FAILURE);
        }

        if (play_cursor >= write_cursor)
            return play_cursor - write_cursor;
        return _file_header.buffer_size - write_cursor + play_cursor;
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
}

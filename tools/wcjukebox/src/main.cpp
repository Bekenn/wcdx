#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <cassert>
#include <cstdlib>
#include <cstddef>

#define NOMINMAX
#define INITGUID
#include <dsound.h>
#include <cguid.h>
#include <comip.h>
#include <comdef.h>


#pragma warning(disable: 4838)  // narrowing conversions

enum stream_archive
{
    preflight, postflight, mission
};

struct game_track_desc
{
    stream_archive archive;
    uint8_t track;
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
    uint32_t track_link_offset;
    uint32_t track_link_count;
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
    uint32_t track_link_count;
    uint32_t track_link_index;
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

struct stream_track_link
{
    uint8_t track;
    uint32_t chunk_index;
};
#pragma pack(pop)

_COM_SMARTPTR_TYPEDEF(IDirectSound8, IID_IDirectSound8);
_COM_SMARTPTR_TYPEDEF(IDirectSoundBuffer, IID_IDirectSoundBuffer);
_COM_SMARTPTR_TYPEDEF(IDirectSoundBuffer8, IID_IDirectSoundBuffer8);
_COM_SMARTPTR_TYPEDEF(IDirectSoundNotify8, IID_IDirectSoundNotify8);

template <typename T, size_t length>
constexpr size_t lengthof(const T (&arr)[length])
{
    return length;
}

static constexpr auto end_of_track = uint32_t(-1);
static constexpr auto flight_intensity = 25;

static void load_track(unsigned track);
static void play_track(unsigned track);

static uint32_t chunk_size(const chunk_header& chunk);
static DWORD buffer_size_available();
static void stream_chunks(const chunk_header*& chunk, uint32_t& chunk_offset, uint8_t tag, char* p, size_t length);
static uint32_t next_chunk_index(uint32_t chunk_index, uint8_t next_track, uint8_t intensity);

static constexpr const char* stream_filenames[]
{
    "STREAMS\\PREFLITE.STR",
    "STREAMS\\POSFLITE.STR",
    "STREAMS\\MISSION.STR"
};

// Maps from a game track number to a stream track number
// See StreamLoadTrack and GetStreamTrack in Wing1.i64
static const game_track_desc track_map[] =
{
    { stream_archive::mission, 5 },         // 0 - Escort Mission
    { stream_archive::mission, 7 },         // 1 - Wingman Lost
    { stream_archive::mission, 7 },         // 2 - Wingman Lost (again, same as 1)
    { stream_archive::mission, 8 },         // 3 - Dire Straits
    { stream_archive::mission, 9 },         // 4 - Momentary Victory
    { stream_archive::mission, 6 },         // 5 - Successful Combat
    // 6-11 are direct links to triggers, should look at actual data
    { stream_archive::mission, 15 },        // 6 - Escort Mission (trigger)
    { stream_archive::mission, 13 },        // 7 - Escort Mission (trigger)
    { stream_archive::mission, 16 },        // 8 - Escort Mission (trigger)
    { stream_archive::mission, 14 },        // 9 - Escort Mission (trigger)
    { stream_archive::mission, 17 },        // 10 - Escort Mission (trigger)
    { stream_archive::mission, 18 },        // 11 - Escort Mission (trigger)
    // end triggers
    { stream_archive::mission, 10 },        // 12 - Defeated Fleeing Enemy
    { stream_archive::mission, 12 },        // 13 - Escort Mission (again)
    { stream_archive::mission, 11 },        // 14 - Tragedy
    { stream_archive::mission, 4 },         // 15 - Escort Mission (again)
    { stream_archive::mission, 3 },         // 16 - Escort Mission (again)
    { stream_archive::mission, 1 },         // 17 - Escort Mission (again)
    { stream_archive::mission, 2 },         // 18 - Escort Mission (again)
    { stream_archive::preflight, 4 },       // 19 - OriginFX (actually, Arcade Victory)
    { stream_archive::preflight, 1 },       // 20 - Arcade Mission
    { stream_archive::preflight, 4 },       // 21 - Arcade Victory
    { stream_archive::preflight, 3 },       // 22 - Arcade Death
    { stream_archive::preflight, -1 },      // 23 - Fanfare
    { stream_archive::preflight, 5 },       // 24 - Grim Briefing
    { stream_archive::preflight, 6 },       // 25 - Normal Briefing
    { stream_archive::preflight, 7 },       // 26 - Briefing Dismissed
    { stream_archive::mission, -1 },        // 27 - Escort Mission (trigger)
    { stream_archive::postflight, -1, },    // 28 - landing
    { stream_archive::postflight, 0, },     // 29 - damage assessment
    { stream_archive::preflight, 0 },       // 30 - rec room
    { stream_archive::mission, 19 },        // 31 - Escort Mission (trigger)
    { stream_archive::mission, 20 },        // 32 - Escort Mission (trigger)
    { stream_archive::postflight, 2 },      // 33 - debriefing (successful)
    { stream_archive::postflight, 1 },      // 34 - debriefing (failed)
    { stream_archive::preflight, 2 },       // 35 - barracks
    { stream_archive::postflight, 3 },      // 36 - colonel halcyon's office
    { stream_archive::postflight, 4 },      // 37 - medal (valor?)
    { stream_archive::postflight, 5 },      // 38 - medal (yellow sun?)
    { stream_archive::postflight, 7 },      // 39 - another medal
    { stream_archive::postflight, 6 },      // 40 - big medal
};

static std::filebuf stream_file;
static stream_file_header file_header;

static std::vector<chunk_header> chunks;
static std::vector<stream_chunk_link> chunk_links;
static std::vector<stream_track_link> track_links;

static IDirectSound8Ptr ds8;
static IDirectSoundBufferPtr dsbuffer_primary;
static IDirectSoundBuffer8Ptr dsbuffer8;
static HANDLE position_event;


int main(int argc, char* argv[])
{
    if (argc == 1)
    {
        std::cout << "Usage: " << argv[0] << " <tracknum>\n";
        return EXIT_SUCCESS;
    }

    if (argc != 2)
    {
        std::cerr << "Bad input.\n";
        return EXIT_FAILURE;
    }

    // DirectSound needs a window handle for SetCooperativeLevel (nullptr doesn't work).
    auto window = ::CreateWindow(L"BUTTON", L"Hidden DirectSound Window", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);
    auto hr = ::DirectSoundCreate8(&DSDEVID_DefaultPlayback, &ds8, nullptr);
    if (FAILED(hr))
    {
        std::cerr << "Couldn't initialize DirectSound.\n";
        return EXIT_FAILURE;
    }

    hr = ds8->SetCooperativeLevel(window, DSSCL_PRIORITY);
    if (FAILED(hr))
    {
        std::cerr << "Couldn't set DirectSound cooperative level.\n";
        return EXIT_FAILURE;
    }

    DSBUFFERDESC desc =
    {
        sizeof(desc),
        DSBCAPS_PRIMARYBUFFER,
        0, 0, nullptr,
        DS3DALG_DEFAULT
    };

    hr = ds8->CreateSoundBuffer(&desc, &dsbuffer_primary, nullptr);
    if (FAILED(hr))
    {
        std::cerr << "Couldn't create DirectSound primary buffer.\n";
        return EXIT_FAILURE;
    }

    auto track = atoi(argv[1]);
    load_track(track);
    play_track(track);
}

void load_track(unsigned track)
{
    const char* stream_name = stream_filenames[track_map[track].archive];

    if (stream_name == nullptr)
        return;

    if (stream_file.open(stream_name, std::ios_base::binary | std::ios_base::in) == nullptr)
        return;

    stream_file.sgetn(reinterpret_cast<char*>(&file_header), sizeof(file_header));

    chunks.resize(file_header.chunk_count);
    stream_file.pubseekoff(file_header.chunk_headers_offset, std::ios_base::beg);
    stream_file.sgetn(reinterpret_cast<char*>(chunks.data()), chunks.size() * sizeof(chunk_header));

    chunk_links.resize(file_header.chunk_link_count);
    stream_file.pubseekoff(file_header.chunk_link_offset, std::ios_base::beg);
    stream_file.sgetn(reinterpret_cast<char*>(chunk_links.data()), chunk_links.size() * sizeof(stream_chunk_link));

    track_links.resize(file_header.track_link_count);
    stream_file.pubseekoff(file_header.track_link_offset, std::ios_base::beg);
    stream_file.sgetn(reinterpret_cast<char*>(track_links.data()), track_links.size() * sizeof(stream_track_link));

    PCMWAVEFORMAT format =
    {
        WAVE_FORMAT_PCM,
        file_header.channels,
        file_header.sample_rate,
        file_header.sample_rate * file_header.channels * (file_header.bits_per_sample / 8),
        file_header.channels * (file_header.bits_per_sample / 8),
        file_header.bits_per_sample
    };

    auto hr = dsbuffer_primary->SetFormat(reinterpret_cast<LPWAVEFORMATEX>(&format));
    if (FAILED(hr))
    {
        std::cerr << "Couldn't set primary buffer output format.\n";
        exit(EXIT_FAILURE);
    }

    DSBUFFERDESC desc =
    {
        sizeof(desc),
        DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY,
        file_header.buffer_size,
        0,
        reinterpret_cast<WAVEFORMATEX*>(&format),
        DS3DALG_DEFAULT
    };

    IDirectSoundBufferPtr dsbuffer;
    hr = ds8->CreateSoundBuffer(&desc, &dsbuffer, nullptr);
    dsbuffer8 = std::move(dsbuffer);
    if (FAILED(hr) || dsbuffer8 == nullptr)
    {
        std::cerr << "Couldn't create sound buffer.\n";
        exit(EXIT_FAILURE);
    }

    auto notify = IDirectSoundNotify8Ptr(dsbuffer8);
    if (notify == nullptr)
    {
        std::cerr << "Couldn't create Notify interface.\n";
        exit(EXIT_FAILURE);
    }

    position_event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (position_event == nullptr)
    {
        std::cerr << "Couldn't create event object.\n";
        exit(EXIT_FAILURE);
    }

    DSBPOSITIONNOTIFY positions[] =
    {
        { 0, position_event },
        { file_header.buffer_size / 2, position_event },
    };
    hr = notify->SetNotificationPositions(lengthof(positions), positions);
    if (FAILED(hr))
    {
        std::cerr << "Couldn't set notification positions.\n";
        exit(EXIT_FAILURE);
    }
}

void play_track(unsigned track)
{
    auto stream_track = track_map[track].track;
    auto index = next_chunk_index(0, stream_track, flight_intensity);
    if (index == end_of_track)
        return;

    stream_track = -1;

    const chunk_header* chunk = &chunks[index];
    uint32_t chunk_offset = 0;

    void* buffer;
    DWORD size;
    auto hr = dsbuffer8->Lock(0, file_header.buffer_size, &buffer, &size, nullptr, nullptr, 0);
    if (FAILED(hr))
    {
        std::cerr << "Couldn't lock buffer.\n";
        exit(EXIT_FAILURE);
    }
    stream_chunks(chunk, chunk_offset, stream_track, static_cast<char*>(buffer), size);
    dsbuffer8->Unlock(buffer, size, nullptr, 0);

    hr = dsbuffer8->Play(0, 0, DSBPLAY_LOOPING);
    if (FAILED(hr))
    {
        std::cerr << "Couldn't play stream.\n";
        exit(EXIT_FAILURE);
    }

    // Consume one event because we have two full sections in the buffer.
    ::WaitForSingleObject(position_event, INFINITE);
    ::ResetEvent(position_event);

    bool done = false;
    while (!done)
    {
        auto wait_result = ::WaitForSingleObject(position_event, INFINITE);
        if (wait_result == WAIT_FAILED)
        {
            std::cerr << "Wait failed.\n";
            exit(EXIT_FAILURE);
        }
        ::ResetEvent(position_event);

        DWORD play, write;
        dsbuffer8->GetCurrentPosition(&play, &write);
        auto midpoint = file_header.buffer_size / 2;
        auto buffer_offset = play < midpoint ? midpoint : 0;
        size = buffer_offset == 0 ? midpoint : file_header.buffer_size - midpoint;
        dsbuffer8->Lock(buffer_offset, size, &buffer, &size, nullptr, nullptr, 0);
        if (chunk == nullptr)
        {
            memset(buffer, 0, size);
            done = true;
        }
        else
            stream_chunks(chunk, chunk_offset, stream_track, static_cast<char*>(buffer), size);
        dsbuffer8->Unlock(buffer, size, nullptr, 0);
    }

    ::WaitForSingleObject(position_event, INFINITE);
    ::ResetEvent(position_event);
    dsbuffer8->Stop();
}

DWORD buffer_size_available()
{
    DWORD play_cursor;
    DWORD write_cursor;
    auto hr = dsbuffer8->GetCurrentPosition(&play_cursor, &write_cursor);
    if (FAILED(hr))
    {
        std::cerr << "Couldn't get buffer size.\n";
        exit(EXIT_FAILURE);
    }

    if (play_cursor >= write_cursor)
        return play_cursor - write_cursor;
    return file_header.buffer_size - write_cursor + play_cursor;
}

uint32_t chunk_size(const chunk_header& chunk)
{
    return chunk.end_offset - chunk.start_offset;
}

void stream_chunks(const chunk_header*& chunk, uint32_t& chunk_offset, uint8_t next_track, char* p, size_t length)
{
    while (length != 0)
    {
        stream_file.pubseekoff(chunk->start_offset + chunk_offset, std::ios_base::beg);
        auto bytes = std::min(chunk_size(*chunk) - chunk_offset, length);
        stream_file.sgetn(p, bytes);
        p += bytes;
        length -= bytes;
        chunk_offset += bytes;
        if (chunk_offset == chunk_size(*chunk))
        {
            chunk_offset = 0;

            auto index = next_chunk_index(chunk - &chunks.front(), next_track, flight_intensity);
            if (index == end_of_track)
            {
                chunk = nullptr;
                memset(p, 0, length);
                return;
            }

            chunk = &chunks[index];
        }
    }
}

uint32_t next_chunk_index(uint32_t chunk_index, uint8_t next_track, uint8_t intensity)
{
    const auto& chunk = chunks[chunk_index];
    auto track_link_first = begin(track_links) + chunk.track_link_index;
    auto track_link_last = track_link_first + chunk.track_link_count;
    for (; track_link_first != track_link_last; ++track_link_first)
    {
        switch (track_link_first->track)
        {
        case 64:
            std::cout << "End of stream.\n";
            return uint32_t(-1);
        case 65:
            std::cout << "Go to prior track.\n";
            return uint32_t(-1);
        default:
            if (track_link_first->track == next_track)
                return track_link_first->chunk_index;
            break;
        }
    }

    auto chunk_link_first = begin(chunk_links) + chunk.chunk_link_index;
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

    if (++chunk_index == chunks.size())
        chunk_index = 0;

    return chunk_index;
}

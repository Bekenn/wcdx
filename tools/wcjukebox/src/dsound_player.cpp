#include "dsound_player.h"
#include "dsound_error.h"

#include <stdext/utility.h>

#include <InitGuid.h>
DEFINE_GUID(DSDEVID_DefaultPlayback, 0xdef00000, 0x9c6d, 0x47ed, 0xaa, 0xf1, 0x4d, 0xda, 0x8f, 0x2b, 0x5c, 0x03);
DEFINE_GUID(IID_IDirectSoundBuffer8, 0x6825a449, 0x7524, 0x4d82, 0x92, 0x0f, 0x50, 0xe3, 0x6a, 0xb3, 0xab, 0x1e);
DEFINE_GUID(IID_IDirectSoundNotify, 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x8, 0x0, 0xa0, 0xc9, 0x25, 0xcd, 0x16);


namespace
{
    HWND DirectSoundWindow()
    {
        static auto window = ::CreateWindow(L"BUTTON", L"Hidden DirectSound Window", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);
        return window;
    }
}

dsound_player::dsound_player()
{
    // DirectSound needs a window handle for SetCooperativeLevel (nullptr doesn't work).
    auto window = DirectSoundWindow();
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

    _position_event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (_position_event == nullptr)
        throw std::system_error(::GetLastError(), std::system_category());
}

dsound_player::~dsound_player()
{
    ::CloseHandle(_position_event);
}

void dsound_player::reset(uint16_t channels, uint32_t sample_rate, uint16_t bits_per_sample, uint32_t buffer_size)
{
    auto bytes_per_sample = (bits_per_sample + 7) / 8;
    PCMWAVEFORMAT format =
    {
        {
            WAVE_FORMAT_PCM,
            channels,
            sample_rate,
            DWORD(sample_rate * channels * bytes_per_sample),
            WORD(channels * bytes_per_sample)
        },
        bits_per_sample
    };

    HRESULT hr = _dsbuffer_primary->SetFormat(reinterpret_cast<LPWAVEFORMATEX>(&format));
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    // Force an even buffer size so that chunk_size (in play()) can be exactly half.  In practice,
    // this will never change the value.
    buffer_size &= ~uint32_t(1);

    DSBUFFERDESC desc =
    {
        sizeof(desc),
        DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY,
        buffer_size,
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
}

void dsound_player::play(stdext::input_stream& stream)
{
    DSBCAPS caps = { sizeof(caps) };
    auto hr = _dsbuffer8->GetCaps(&caps);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    auto chunk_size = caps.dwBufferBytes / 2;

    auto notify = IDirectSoundNotify8Ptr(_dsbuffer8);
    if (notify == nullptr)
        _com_raise_error(E_NOINTERFACE);

    DSBPOSITIONNOTIFY positions[] =
    {
        { 0, _position_event },
        { chunk_size, _position_event },
    };
    hr = notify->SetNotificationPositions(stdext::lengthof(positions), positions);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    auto done = fill_buffer(0, stream, chunk_size) == 0;
    if (done)
        return;

    hr = _dsbuffer8->Play(0, 0, DSBPLAY_LOOPING);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    while (!done)
    {
        auto wait_result = ::WaitForSingleObject(_position_event, INFINITE);
        if (wait_result == WAIT_FAILED)
            throw std::system_error(::GetLastError(), std::system_category());

        DWORD play;
        hr = _dsbuffer8->GetCurrentPosition(&play, nullptr);
        if (FAILED(hr))
            throw std::system_error(hr, dsound_category());

        auto buffer_offset = play < chunk_size ? chunk_size : 0;
        done = fill_buffer(buffer_offset, stream, chunk_size) == 0;
    }

    ::WaitForSingleObject(_position_event, INFINITE);
    _dsbuffer8->Stop();
}

size_t dsound_player::fill_buffer(uint32_t offset, stdext::input_stream& stream, uint32_t size)
{
    uint8_t* buffer;
    DWORD buffer_bytes;
    auto hr = _dsbuffer8->Lock(offset, size, reinterpret_cast<LPVOID*>(&buffer), &buffer_bytes, nullptr, nullptr, 0);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());
    at_scope_exit([&] { _dsbuffer8->Unlock(buffer, buffer_bytes, nullptr, 0); });

    auto bytes = stream.read(buffer, buffer_bytes);
    std::fill_n(buffer + bytes, buffer_bytes - bytes, 0);
    return bytes;
}

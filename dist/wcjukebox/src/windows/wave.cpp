#include "wave.h"
#include "dsound_error.h"

#include <stdext/scope_guard.h>
#include <stdext/string.h>
#include <stdext/utility.h>

#define NOMINMAX
#include <Windows.h>
#include <InitGuid.h>
#include <dsound.h>
#include <comdef.h>


_COM_SMARTPTR_TYPEDEF(IDirectSound8, IID_IDirectSound8);
_COM_SMARTPTR_TYPEDEF(IDirectSoundBuffer, IID_IDirectSoundBuffer);
_COM_SMARTPTR_TYPEDEF(IDirectSoundBuffer8, IID_IDirectSoundBuffer8);
_COM_SMARTPTR_TYPEDEF(IDirectSoundNotify8, IID_IDirectSoundNotify8);

namespace
{
    HWND DirectSoundWindow();
    size_t fill_buffer(IDirectSoundBuffer8* dsbuffer8, uint32_t offset, stdext::input_stream& stream, uint32_t size);
}

void play_wave(stdext::input_stream& in, uint16_t channels, uint32_t sample_rate, uint16_t bits_per_sample, size_t buffer_size)
try
{
    IDirectSound8Ptr ds8;
    auto hr = ::DirectSoundCreate8(&DSDEVID_DefaultPlayback, &ds8, nullptr);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    // DirectSound needs a window handle for SetCooperativeLevel (nullptr doesn't work).
    hr = ds8->SetCooperativeLevel(DirectSoundWindow(), DSSCL_PRIORITY);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    DSBUFFERDESC desc =
    {
        sizeof(desc),
        DSBCAPS_PRIMARYBUFFER,
        0, 0, nullptr,
        DS3DALG_DEFAULT
    };

    IDirectSoundBufferPtr dsbuffer_primary;
    hr = ds8->CreateSoundBuffer(&desc, &dsbuffer_primary, nullptr);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

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

    hr = dsbuffer_primary->SetFormat(reinterpret_cast<LPWAVEFORMATEX>(&format));
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    // Force an even buffer size so that chunk_size can be exactly half.  In practice,
    // this will never change the value.
    buffer_size &= ~uint32_t(1);
    auto chunk_size = uint32_t(buffer_size / 2);

    desc =
    {
        sizeof(desc),
        DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY,
        DWORD(buffer_size),
        0,
        reinterpret_cast<WAVEFORMATEX*>(&format),
        DS3DALG_DEFAULT
    };

    IDirectSoundBufferPtr dsbuffer;
    hr = ds8->CreateSoundBuffer(&desc, &dsbuffer, nullptr);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    IDirectSoundBuffer8Ptr dsbuffer8 = std::move(dsbuffer);
    if (dsbuffer8 == nullptr)
        _com_raise_error(E_NOINTERFACE);

    auto notify = IDirectSoundNotify8Ptr(dsbuffer8);
    if (notify == nullptr)
        _com_raise_error(E_NOINTERFACE);

    auto position_event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (position_event == nullptr)
        throw std::system_error(::GetLastError(), std::system_category());
    at_scope_exit([&] { ::CloseHandle(position_event); });

    DSBPOSITIONNOTIFY positions[] =
    {
        { 0, position_event },
        { chunk_size, position_event },
    };
    hr = notify->SetNotificationPositions(DWORD(stdext::lengthof(positions)), positions);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());

    auto done = fill_buffer(dsbuffer8, 0, in, chunk_size) == 0;
    if (done)
        return;

    hr = dsbuffer8->Play(0, 0, DSBPLAY_LOOPING);
    if (FAILED(hr))
        throw std::system_error(hr, dsound_category());
    at_scope_exit([&]{ dsbuffer8->Stop(); });

    while (!done)
    {
        auto wait_result = ::WaitForSingleObject(position_event, INFINITE);
        if (wait_result == WAIT_FAILED)
            throw std::system_error(::GetLastError(), std::system_category());

        DWORD play;
        hr = dsbuffer8->GetCurrentPosition(&play, nullptr);
        if (FAILED(hr))
            throw std::system_error(hr, dsound_category());

        auto buffer_offset = play < chunk_size ? chunk_size : 0;
        done = fill_buffer(dsbuffer8, buffer_offset, in, chunk_size) == 0;
    }

    ::WaitForSingleObject(position_event, INFINITE);
}
catch (const _com_error& e)
{
    // Convert the exception to runtime_error so that main can catch it.
    throw std::runtime_error(stdext::to_mbstring(e.ErrorMessage()));
}

namespace
{
    HWND DirectSoundWindow()
    {
        static auto window = ::CreateWindow(L"BUTTON", L"Hidden DirectSound Window", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);
        return window;
    }

    size_t fill_buffer(IDirectSoundBuffer8* dsbuffer8, uint32_t offset, stdext::input_stream& stream, uint32_t size)
    {
        std::byte* buffer;
        DWORD buffer_bytes;
        auto hr = dsbuffer8->Lock(offset, size, reinterpret_cast<LPVOID*>(&buffer), &buffer_bytes, nullptr, nullptr, 0);
        if (FAILED(hr))
            throw std::system_error(hr, dsound_category());
        at_scope_exit([&] { dsbuffer8->Unlock(buffer, buffer_bytes, nullptr, 0); });

        auto bytes = stream.read(buffer, buffer_bytes);
        std::fill_n(buffer + bytes, buffer_bytes - bytes, std::byte());
        return bytes;
    }
}

#pragma once

#include <stdext/multi.h>
#include <stdext/stream.h>

#define NOMINMAX
#include <Windows.h>
#include <dsound.h>
#include <comdef.h>


_COM_SMARTPTR_TYPEDEF(IDirectSound8, IID_IDirectSound8);
_COM_SMARTPTR_TYPEDEF(IDirectSoundBuffer, IID_IDirectSoundBuffer);
_COM_SMARTPTR_TYPEDEF(IDirectSoundBuffer8, IID_IDirectSoundBuffer8);
_COM_SMARTPTR_TYPEDEF(IDirectSoundNotify8, IID_IDirectSoundNotify8);

class dsound_player
{
public:
    explicit dsound_player();
    ~dsound_player();

public:
    void reset(uint16_t channels, uint32_t sample_rate, uint16_t bits_per_sample, uint32_t buffer_size);
    void play(stdext::input_stream& stream);

private:
    size_t dsound_player::fill_buffer(uint32_t offset, stdext::input_stream& stream, uint32_t size);

private:
    IDirectSound8Ptr _ds8;
    IDirectSoundBufferPtr _dsbuffer_primary;
    IDirectSoundBuffer8Ptr _dsbuffer8;
    HANDLE _position_event;
};

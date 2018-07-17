#pragma once

#include "player.h"

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

public:
    void load(stdext::multi_ref<stdext::input_stream, stdext::seekable> stream);
    void play(uint8_t trigger, uint8_t intensity);

private:
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

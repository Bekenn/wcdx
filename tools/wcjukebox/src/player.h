#pragma once

#include <cstdint>


constexpr auto end_of_track = uint32_t(-1);
constexpr auto no_trigger = uint8_t(-1);

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

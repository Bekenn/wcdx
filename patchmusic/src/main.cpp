#include <fstream>
#include <iostream>

#include <cstddef>


struct chunk_header
{
    uint32_t start_offset;
    uint32_t end_offset;
    uint32_t track_link_count;
    uint32_t track_link_index;
    uint32_t chunk_link_count;
    uint32_t chunk_link_index;
};

template <typename T, size_t length>
constexpr size_t lengthof(const T (&)[length])
{
    return length;
}

static void show_usage(const wchar_t* name);
bool deloop_chunk(std::filebuf& file, uint32_t offset, const chunk_header& target_chunk);


static constexpr uint32_t target_chunk_offsets[] =
{
    0x03A1C7BC,
    0x03A1C894
};

static constexpr chunk_header target_chunks[] =
{
    {
        0x01A28DA0, 0x01A40058,
        0, 2565,
        1, 3419
    },
    {
        0x01AEAA68, 0x01B04410,
        0, 2565,
        1, 3433
    }
};

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        show_usage(argc == 1 ? argv[0] : L"patchmusic");
        return EXIT_SUCCESS;
    }

    if (argc != 2)
    {
        std::cerr << "Error: Unrecognized arguments.\n";
        show_usage(argv[0]);
        return EXIT_FAILURE;
    }

    std::filebuf file;
    if (file.open(argv[1], std::ios_base::in | std::ios_base::out | std::ios_base::binary) == nullptr)
    {
        std::wcerr << L"Error: Unable to open file: " << argv[1] << L"\n";
        return EXIT_FAILURE;
    }

    for (size_t n = 0; n < lengthof(target_chunks); ++n)
    {
        if (!deloop_chunk(file, target_chunk_offsets[n], target_chunks[n]))
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

bool deloop_chunk(std::filebuf& file, uint32_t offset, const chunk_header& target_chunk)
{
    auto position = file.pubseekoff(offset, std::ios_base::beg);
    if (position == std::streampos(std::streamoff(-1)))
    {
        std::wcerr << L"Error: Seek error.\n";
        return false;
    }

    chunk_header header;
    if (file.sgetn(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header))
    {
        std::wcerr << L"Error: Unexpected end of file.\n";
        return false;
    }

    if (memcmp(&header, &target_chunk, sizeof(chunk_header)) != 0)
    {
        std::wcerr << L"Error: Bytes in stream chunk header do not match expected values.\n";
        return false;
    }

    header.chunk_link_count = 0;
    header.track_link_count = 1;
    header.track_link_index = 3873;

    file.pubseekpos(position);
    if (file.sputn(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header))
    {
        std::wcerr << L"Error: Couldn't write to file.\n";
        return false;
    }

    return true;
}

void show_usage(const wchar_t* name)
{
    std::wcout << L"Usage: " << name << L" <path-to-missions.str>\n";
}

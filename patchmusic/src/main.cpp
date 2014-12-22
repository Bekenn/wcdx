#include <fstream>
#include <iostream>

#include <cstddef>


using namespace std;

struct chunk_header
{
	uint32_t start_offset;
	uint32_t end_offset;
	uint32_t track_link_count;
	uint32_t track_link_index;
	uint32_t chunk_link_count;
	uint32_t chunk_link_index;
};

static void show_usage(const wchar_t* name);

static constexpr chunk_header target_chunk =
{
	0x01AEAA68, 0x01B04410,
	0, 2565,
	1, 3433
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
		cerr << "Error: Unrecognized arguments." << endl;
		show_usage(argv[0]);
		return EXIT_FAILURE;
	}

	filebuf file;
	if (file.open(argv[1], ios_base::in | ios_base::out | ios_base::binary) == nullptr)
	{
		wcerr << L"Error: Unable to open file: " << argv[1] << endl;
		return EXIT_FAILURE;
	}

	auto position = file.pubseekoff(0x03a1c894, ios_base::beg);
	if (position == streampos(streamoff(-1)))
	{
		wcerr << L"Error: Seek error." << endl;
		return EXIT_FAILURE;
	}

	chunk_header header;
	if (file.sgetn(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header))
	{
		wcerr << L"Error: Unexpected end of file." << endl;
		return EXIT_FAILURE;
	}

	if (memcmp(&header, &target_chunk, sizeof(chunk_header)) != 0)
	{
		wcerr << L"Error: Bytes in stream chunk header do not match expected values." << endl;
		return EXIT_FAILURE;
	}

	header.chunk_link_count = 0;
	header.track_link_count = 1;
	header.track_link_index = 3873;

	file.pubseekpos(position);
	if (file.sputn(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header))
	{
		wcerr << L"Error: Couldn't write to file." << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void show_usage(const wchar_t* name)
{
	wcout << L"Usage: " << name << L" <path-to-missions.str>" << endl;
}

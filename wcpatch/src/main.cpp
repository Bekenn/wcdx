#include "common.h"
#include "resource_stream.h"
#include "md5.h"
#include "../res/resources.h"

#include "iolib/file_stream.h"
#include "iolib/string_view.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>


using namespace std;
using namespace iolib;

struct section_header_t
{
	char name[8];
	uint32_t virtual_size;
	uint32_t virtual_address;
	uint32_t raw_data_size;
	uint32_t raw_data_offset;
	uint32_t relocations_offset;
	uint32_t line_numbers_offset;
	uint16_t relocation_count;
	uint16_t line_number_count;
	uint32_t characteristics;
};

struct import_entry_t
{
	uint32_t lookup_virtual_address;
	uint32_t timestamp;
	uint32_t forwarder_chain;
	uint32_t dllname_virtual_address;
	uint32_t import_table_virtual_address;
};

static void show_usage(const wchar_t* invocation);

static bool patch_image(seekable_stream& file_data);
static bool apply_dif(seekable_stream& file_data, md5_hash hash);

static string read_line(seekable_input_stream& is);

static const import_entry_t import_entry_null = { };

static const char PESignature[] = { 'P', 'E', '\0', '\0' };
static const uint16_t OptionalHeader_PE32Signature = 0x10B;
static const uint16_t OptionalHeader_PE32PlusSignature = 0x20B;

int wmain(int argc, wchar_t* argv[])
{
#if _DEBUG
	at_scope_exit([&]{ system("pause"); });
#endif

	const wchar_t* input_path = nullptr;
	const wchar_t* output_path = nullptr;
	bool headers_only = false;

	for (const wchar_t* const* arg = argv + 1; *arg != nullptr; ++arg)
	{
		switch (**arg)
		{
		case L'\0':
			continue;

		case L'-':
		case L'/':
			if (wcscmp(*arg + 1, L"headers-only") == 0)
				headers_only = true;
			break;

		default:
			if (input_path == nullptr)
				input_path = *arg;
			else if (output_path == nullptr)
				output_path = *arg;
			else
			{
				show_usage(argv[0]);
				return EXIT_FAILURE;
			}
		}
	}

	if (input_path == nullptr)
	{
		cerr << "No input file specified." << endl;
		show_usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (output_path == nullptr)
	{
		cerr << "No output file specified." << endl;
		show_usage(argv[0]);
		return EXIT_FAILURE;
	}

	try
	{
		// Read the input file into an in-memory buffer.
		vector<uint8_t> file_buffer;
		{
			file input_file(input_path, file::mode::open | file::mode::read);
			input_file.seek(0, file::seek_from::end);
			size_t size = size_t(input_file.position());
			file_buffer.resize(size);
			input_file.seek(0, file::seek_from::beginning);
			input_file.read(file_buffer.data(), file_buffer.size());
		}

 		auto hash = md5_hash(file_buffer.data(), file_buffer.size());

		// iolib streams are very good for reading and writing heterogeneous data.
		memory_stream file_data(file_buffer.data(), file_buffer.size());

		if (!patch_image(file_data))
			return EXIT_FAILURE;

		if (!headers_only && !apply_dif(file_data, hash))
			return EXIT_FAILURE;

		file output_file(output_path, file::mode::open_or_create | file::mode::write);
		output_file.write(file_buffer.data(), file_buffer.size());
		return EXIT_SUCCESS;
	}
	catch (const exception& e)
	{
		cerr << e.what() << endl;
	}
	catch (...)
	{
	}

	return EXIT_FAILURE;
}

void show_usage(const wchar_t* invocation)
{
	wcout << L"Usage:\n\t" << invocation << L" <input_path> <output_path>" << endl;
}

bool patch_image(seekable_stream& file_data)
{
	// Read offset of PE header
	file_data.seek(0x3C);
	uint32_t offset;
	file_data.read(offset);

	// Read PE signature
	file_data.seek(offset, seekable_stream::seek_from::beginning);
	char signature[4];
	file_data.read(signature);
	if (!equal(begin(signature), end(signature), begin(PESignature)))
	{
		wcerr << L"Error: Input file is not a valid executable." << endl;
		return false;
	}

	// Read number of sections
	file_data.seek(2);
	uint16_t section_count;
	file_data.read(section_count);

	// Read optional header size
	file_data.seek(12);
	uint16_t header_size;
	file_data.read(header_size);

	// Set the IMAGE_FILE_RELOCS_STRIPPED flag
	uint16_t flags;
	file_data.read(flags);
	file_data.seek(-2);
	file_data.write(uint16_t(flags | 1));

	uint16_t pe_type_signature;
	file_data.read(pe_type_signature);
	if (pe_type_signature != OptionalHeader_PE32Signature && pe_type_signature != OptionalHeader_PE32PlusSignature)
	{
		wcerr << L"Error: Input file is not a valid executable." << endl;
		return false;
	}

	// Set the minimum OS fields to Windows XP
	file_data.seek(38);
	file_data.write(uint16_t(5));
	file_data.write(uint16_t(1));
	file_data.seek(4);
	file_data.write(uint16_t(5));
	file_data.write(uint16_t(1));

	// Set the NX-compatible bit
	file_data.seek(18);
	file_data.read(flags);
	flags |= 0x0100;
	file_data.seek(-2);
	file_data.write(flags);

	// Skip to the data directories and clear out the relocation table entry.
	file_data.seek(20);
	uint32_t count;
	file_data.read(count);
	if (count >= 6)
	{
		file_data.seek(5 * 8);
		file_data.write(uint32_t(0));	// relocation data offset
		file_data.write(uint32_t(0));	// relocation data size

		// Skip to the section tables.
		file_data.seek(header_size - 96 - (6 * 8));	// seek past the optional header
	}
	else
		file_data.seek(header_size - 96);	// Skip to the section tables.

	section_header_t section_header;
	for (unsigned section_header_index = 0; section_header_index < section_count; ++section_header_index)
	{
		file_data.read(section_header);
		if (strcmp(section_header.name, ".idata") == 0)
			break;
	}

	// Skip to the import tables.
	file_data.seek(section_header.raw_data_offset, file::seek_from::beginning);

	// The import tables specify RVAs for import entries, so we'll need the base
	// address of the section in order to locate the imports in the input file.
	uint32_t idata_base_rva = section_header.virtual_address;

	// Skip past the directory tables.
	import_entry_t import_entry;
	while (true)
	{
		file_data.read(import_entry);
		if (memcmp(&import_entry, &import_entry_null, sizeof(import_entry_t)) == 0)
			return false;

		auto import_entry_position = file_data.position();
		file_data.seek(section_header.raw_data_offset + import_entry.dllname_virtual_address - idata_base_rva, file::seek_from::beginning);
		auto import_name_position = file_data.position();
		string dll_name;
		char ch;
		while ((file_data.read(ch), ch) != '\0')
			dll_name += ch;
		if (_stricmp(dll_name.c_str(), "ddraw.dll") == 0)
		{
			file_data.set_position(import_name_position);
			const char import_name[] = "wcdx.dll";
			file_data.write(import_name, lengthof(import_name));
			break;
		}

		file_data.set_position(import_entry_position);
	}

	file_data.seek(section_header.raw_data_offset + import_entry.lookup_virtual_address - idata_base_rva, file::seek_from::beginning);
	if (pe_type_signature != OptionalHeader_PE32Signature)
		return false;

	uint32_t lookup;
	memory_stream::position_type lookup_position;
	while (true)
	{
		if (!file_data.read(lookup) || lookup == 0)
			return false;

		if ((lookup & 0x80000000) == 0)
		{
			lookup_position = file_data.position();
			file_data.seek(section_header.raw_data_offset + lookup - idata_base_rva + 2, file::seek_from::beginning);
			auto name_position = file_data.position();

			string name;
			char ch;
			while ((file_data.read(ch), ch) != '\0')
				name += ch;
			if (name == "DirectDrawCreate")
			{
				file_data.set_position(name_position);
				file_data.seek(-2);
				break;
			}
			file_data.set_position(lookup_position);
		}
	}

	file_data.write(uint16_t(0));
	string_view function_name = "WcdxCreate";
	file_data.write(function_name.data(), function_name.length() * sizeof(char));
	file_data.write('\0');

	file_data.set_position(lookup_position);
	file_data.write(0);
	return true;
}

bool apply_dif(seekable_stream& file_data, md5_hash hash)
{
	constexpr map<md5_hash, uint32_t> diffs =
	{
		{ { 0xccc8b82d, 0x79ef51fa, 0xcd158646, 0xf4ab94d1 }, RESOURCE_ID_WING1_DIFF },
		{ { 0x180491e6, 0x8ae91e28, 0xde8e99e3, 0xb0854881 }, RESOURCE_ID_TRANSFER_DIFF }
	};

	auto i = diffs.find(hash);
	if (i == diffs.end())
		return false;

	resource_stream resource(i->second);
	string line = read_line(resource);
	if (line != "This difference file has been created by IDA Pro")
		return false;

	while (!resource.at_end())
	{
		line = read_line(resource);
		if (line.empty())
			continue;

		auto n = line.find(':');
		if (n == string::npos)
			continue;
		string_view address_str(line, 0, n);

		n = line.find_first_not_of(' ', n + 1);
		if (n == string::npos)
			return false;
		auto m = line.find(' ', n);
		if (n == string::npos)
			return false;
		string_view original_value_str(line, n, m - n);

		n = line.find_first_not_of(' ', m + 1);
		if (n == string::npos)
			return false;
		m = line.find(' ', n);
		if (m == string::npos)
			m = line.length();
		string_view replacement_value_str(line, n, m - n);

		if (line.find_first_not_of(' ', m + 1) != string::npos)
			return false;

		if (original_value_str == "FFFFFFFF")
			continue;

		uint32_t offset = stoul(address_str, nullptr, 16);
		uint8_t original_value = uint8_t(stoul(original_value_str, nullptr, 16));
		uint8_t replacement_value = uint8_t(stoul(replacement_value_str, nullptr, 16));

		file_data.seek(offset, seekable_stream::seek_from::beginning);
		uint8_t value;
		if (!file_data.read(value) || value != original_value)
			return false;
		file_data.seek(-1);
		file_data.write(replacement_value);
	}

	return true;
}

string read_line(seekable_input_stream& is)
{
	auto position = is.position();
	size_t length = 0;
	char ch;
	while (is.read(ch) && (ch != '\r') && (ch != '\n'))
		++length;
	is.set_position(position);

	string line;
	line.reserve(length);
	while (is.read(ch) && (ch != '\r') && (ch != '\n'))
		line += ch;

	if ((ch == '\r') && is.read(ch) && (ch != '\n'))
		is.seek(-1);

	return line;
}

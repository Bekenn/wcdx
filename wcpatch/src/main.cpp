#include "common.h"

#include <iolib/file_stream.h>
#include <iolib/stream.h>
#include <windows_error.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


using namespace std;
using namespace iolib;
using namespace windows;

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

static bool patch_imports(stream& file_data);

static const import_entry_t import_entry_null = { };

static const char PESignature[] = { 'P', 'E', '\0', '\0' };
static const uint16_t OptionalHeader_PE32Signature = 0x10B;
static const uint16_t OptionalHeader_PE32PlusSignature = 0x20B;

int wmain(int argc, wchar_t* argv[])
{
#if _DEBUG
	at_scope_exit([&]{ system("pause"); });
#endif

	if (argc != 3)
	{
		wcout << L"Usage:\n\t" << argv[0] << L" <input_path> <output_path>" << endl;
		return EXIT_FAILURE;
	}

	try
	{
		// Read the input file into an in-memory buffer.
		vector<uint8_t> file_buffer;
		{
			file input_file(argv[1], file::mode::open | file::mode::read);
			input_file.seek(0, file::seek_from::end);
			size_t size = size_t(input_file.position());
			file_buffer.resize(size);
			input_file.seek(0, file::seek_from::beginning);
			input_file.read(file_buffer.data(), file_buffer.size());
		}

		// iolib streams are very good for reading and writing heterogeneous data.
		memory_stream file_data(file_buffer.data(), file_buffer.size());

		if (!patch_imports(file_data))
			return EXIT_FAILURE;

		return EXIT_SUCCESS;
	}
	catch (...)
	{
	}

	return EXIT_FAILURE;
}

bool patch_imports(stream& file_data)
{
	// Read offset of PE header
	file_data.seek(0x3C);
	uint32_t offset;
	file_data.read(offset);

	// Read PE signature
	file_data.seek(offset, stream::seek_from::beginning);
	char signature[4];
	file_data.read(signature, sizeof(signature));
	if (!equal(begin(signature), end(signature), begin(PESignature)))
	{
		wcerr << L"Error: Input file is not a valid executable." << endl;
		return false;
	}

	// Read number of sections
	file_data.seek(2);
	uint16_t section_count;
	file_data.read(section_count);

	// Skip to optional header
	file_data.seek(12);
	uint16_t header_size;
	file_data.read(header_size);

	file_data.seek(2);
	uint16_t pe_type_signature;
	file_data.read(pe_type_signature);
	if (pe_type_signature != OptionalHeader_PE32Signature && pe_type_signature != OptionalHeader_PE32PlusSignature)
	{
		wcerr << L"Error: Input file is not a valid executable." << endl;
		return false;
	}

	// Skip to the section tables.
	file_data.seek(header_size - 2);

	section_header_t section_header;
	for (unsigned section_header_index = 0; section_header_index < section_count; ++section_header_index)
	{
		file_data.read(section_header);
		if (strcmp(section_header.name, ".idata") == 0)
			break;
	}

	// Save current position.
	auto position = file_data.position();

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
		string dll_name;
		char ch;
		while ((file_data.read(ch), ch) != '\0')
			dll_name += ch;
		if (_stricmp(dll_name.c_str(), "ddraw.dll") == 0)
			break;

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
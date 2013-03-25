#include "common.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


using namespace std;

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

	FILE* input_file = _wfopen(argv[1], L"rb");
	if (input_file == nullptr)
	{
		wcerr << L"Error: Unable to open input path: " << argv[1] << endl;
		return EXIT_FAILURE;
	}

	vector<uint8_t> file_data;
	{
		at_scope_exit([&]{ fclose(input_file); });
		fseek(input_file, 0, SEEK_END);
		long size = ftell(input_file);
		if (size == -1)
		{
			wcerr << L"Error: Unable to determine input file size." << endl;
			return EXIT_FAILURE;
		}

		file_data.resize(size);
		rewind(input_file);
		fread(file_data.data(), size, 1, input_file);
	}

	auto file_p = file_data.data();

	// Read offset of PE header
	file_p += 0x3C;
	uint32_t offset = *reinterpret_cast<uint32_t*>(file_p);

	// Read PE signature
	file_p = file_data.data() + offset;
	if (memcmp(file_p, PESignature, lengthof(PESignature)) != 0)
	{
		wcerr << L"Error: Input file is not a valid executable." << endl;
		return EXIT_FAILURE;
	}

	// Read number of sections
	file_p += sizeof(PESignature) + 2;
	uint16_t section_count = *reinterpret_cast<uint16_t*>(file_p);

	// Skip to optional header
	file_p += 14;
	uint16_t header_size = *reinterpret_cast<uint16_t*>(file_p);
	file_p += 4;
	uint16_t pe_type_signature = *reinterpret_cast<uint16_t*>(file_p);
	if (pe_type_signature != OptionalHeader_PE32Signature && pe_type_signature != OptionalHeader_PE32PlusSignature)
	{
		wcerr << L"Error: Input file is not a valid executable." << endl;
		return EXIT_FAILURE;
	}

	// Skip to the section tables.
	file_p += header_size;

	for (unsigned section_header_index = 0; section_header_index < section_count; ++section_header_index, file_p += sizeof(section_header_t))
	{
		section_header_t& section_header = *reinterpret_cast<section_header_t*>(file_p);
		if (strcmp(section_header.name, ".idata") != 0)
			continue;

		// Skip to the import tables.
		file_p = file_data.data() + section_header.raw_data_offset;

		// The import tables specify RVAs for import entries, so we'll need the base
		// address of the section in order to locate the imports in the input file.
		uint32_t idata_base_rva = section_header.virtual_address;
		auto idata_base_p = file_p;

		// Skip past the directory tables.
		while (true)
		{
			import_entry_t& import_entry = *reinterpret_cast<import_entry_t*>(file_p);
			if (memcmp(&import_entry, &import_entry_null, sizeof(import_entry_t)) == 0)
				break;

			auto import_entry_p = file_p + sizeof(import_entry_t);
			file_p = idata_base_p + import_entry.dllname_virtual_address - idata_base_rva;
			if (_stricmp(reinterpret_cast<char*>(file_p), "ddraw.dll") == 0)
			{
				file_p = idata_base_p + import_entry.lookup_virtual_address - idata_base_rva;
				if (pe_type_signature == OptionalHeader_PE32Signature)
				{
					uint32_t lookup;
					while ((lookup = *reinterpret_cast<uint32_t*>(file_p)) != 0)
					{
						if ((lookup & 0x80000000) == 0)
						{
							auto name_p = idata_base_p + lookup - idata_base_rva + 2;
							string name = reinterpret_cast<char*>(name_p);
						}
						file_p += sizeof(uint32_t);
					}
				}
				else
				{
					uint64_t lookup;
					while ((lookup = *reinterpret_cast<uint64_t*>(file_p)) != 0)
						file_p += sizeof(uint64_t);
				}
			}

			file_p = import_entry_p;
		}
	}

	return EXIT_SUCCESS;
}

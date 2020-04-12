#include "resource_stream.h"
#include "md5.h"
#include "res/resources.h"

#include <stdext/file.h>
#include <stdext/multi.h>
#include <stdext/string_view.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>


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

static bool patch_image(stdext::multi_ref<stdext::stream, stdext::seekable> file_data);
static bool apply_dif(stdext::multi_ref<stdext::stream, stdext::seekable> file_data, uint32_t hash);

static std::optional<std::string> read_line(stdext::multi_ref<stdext::input_stream, stdext::seekable> is);

static const import_entry_t import_entry_null = { };

static const char PESignature[] = { 'P', 'E', '\0', '\0' };
static const uint16_t OptionalHeader_PE32Signature = 0x10B;
static const uint16_t OptionalHeader_PE32PlusSignature = 0x20B;

int wmain(int argc, wchar_t* argv[])
{
#ifdef _DEBUG
    at_scope_exit([&]{ system("pause"); });
#endif

    try
    {
        const wchar_t* input_path = nullptr;
        const wchar_t* output_path = nullptr;
        bool headers_only = false;

        stdext::discard(argc);
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
            std::cerr << "No input file specified.\n";
            show_usage(argv[0]);
            return EXIT_FAILURE;
        }

        if (output_path == nullptr)
        {
            std::cerr << "No output file specified.\n";
            show_usage(argv[0]);
            return EXIT_FAILURE;
        }

        // Read the input file into an in-memory buffer.
        std::vector<std::byte> file_buffer;
        {
            stdext::file_input_stream input_file(input_path);
            input_file.seek(stdext::seek_from::end, 0);
            size_t size = size_t(input_file.position());
            file_buffer.resize(size);
            input_file.seek(stdext::seek_from::begin, 0);
            input_file.read_all(file_buffer.data(), file_buffer.size());
        }

        auto hash = md5_hash(file_buffer.data(), file_buffer.size());

        // stdext streams are very good for reading and writing heterogeneous data.
        stdext::memory_stream file_data(file_buffer.data(), file_buffer.size());

        if (!patch_image(file_data))
            return EXIT_FAILURE;

        if (!headers_only && !apply_dif(file_data, hash.a ^ hash.b ^ hash.c ^ hash.d))
            return EXIT_FAILURE;

        stdext::file_output_stream output_file(output_path);
        if (output_file.write(file_buffer.data(), file_buffer.size()) != file_buffer.size())
        {
            std::cerr << "Error writing to output file.\n";
            show_usage(argv[0]);
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "Unknown error\n";
    }

    return EXIT_FAILURE;
}

void show_usage(const wchar_t* invocation)
{
    std::wcout << L"Usage:\n\t" << invocation << L" <input_path> <output_path>\n";
}

bool patch_image(stdext::multi_ref<stdext::stream, stdext::seekable> file_data)
{
    auto& stream = file_data.as<stdext::stream>();
    auto& seekable = file_data.as<stdext::seekable>();

    // Read offset of PE header
    seekable.set_position(0x3C);
    auto offset = stream.read<uint32_t>();

    // Read PE signature
    seekable.seek(stdext::seek_from::begin, offset);
    char signature[4];
    if (stream.read(signature) != stdext::lengthof(signature) || !std::equal(std::begin(signature), std::end(signature), std::begin(PESignature)))
    {
        std::wcerr << L"Error: Input file is not a valid executable.\n";
        return false;
    }

    // Read number of sections
    seekable.seek(stdext::seek_from::current, 2);
    auto section_count = stream.read<uint16_t>();
    if (section_count == 0)
    {
        std::wcerr << L"Error: Input file has no sections.\n";
        return false;
    }

    // Read optional header size
    seekable.seek(stdext::seek_from::current, 12);
    auto header_size = stream.read<uint16_t>();

    // Set the IMAGE_FILE_RELOCS_STRIPPED flag
    auto flags = stream.read<uint16_t>();
    seekable.seek(stdext::seek_from::current, -2);
    stream.write(uint16_t(flags | 1));

    auto pe_type_signature = stream.read<uint16_t>();
    if (pe_type_signature != OptionalHeader_PE32Signature && pe_type_signature != OptionalHeader_PE32PlusSignature)
    {
        std::wcerr << L"Error: Input file is not a valid executable.\n";
        return false;
    }

    // Set the minimum OS fields to Windows XP
    seekable.seek(stdext::seek_from::current, 38);
    stream.write(uint16_t(5));
    stream.write(uint16_t(1));
    seekable.seek(stdext::seek_from::current, 4);
    stream.write(uint16_t(5));
    stream.write(uint16_t(1));

    // Set the NX-compatible bit
    seekable.seek(stdext::seek_from::current, 18);
    flags = stream.read<uint16_t>();
    flags |= 0x0100;
    seekable.seek(stdext::seek_from::current, -2);
    stream.write(flags);

    // Skip to the data directories and clear out the relocation table entry.
    seekable.seek(stdext::seek_from::current, 20);
    auto count = stream.read<uint32_t>();
    if (count >= 6)
    {
        seekable.seek(stdext::seek_from::current, 5 * 8);
        stream.write(uint32_t(0));  // relocation data offset
        stream.write(uint32_t(0));  // relocation data size

        // Skip to the section tables.
        seekable.seek(stdext::seek_from::current, header_size - 96 - (6 * 8));  // seek past the optional header
    }
    else
        seekable.seek(stdext::seek_from::current, header_size - 96);    // Skip to the section tables.

    section_header_t section_header = { };  // shouldn't have to initialize here, but MSVC issues C4701 if I don't
    {
        unsigned section_header_index = 0;
        for (; section_header_index < section_count; ++section_header_index)
        {
            section_header = stream.read<section_header_t>();
            if (strcmp(section_header.name, ".idata") == 0)
                break;
        }

        if (section_header_index == section_count)
        {
            std::wcerr << L"Error: Input file has no imports.\n";
            return false;
        }
    }

    // Skip to the import tables.
    seekable.seek(stdext::seek_from::begin, section_header.raw_data_offset);

    // The import tables specify RVAs for import entries, so we'll need the base
    // address of the section in order to locate the imports in the input file.
    uint32_t idata_base_rva = section_header.virtual_address;

    // Skip past the directory tables.
    import_entry_t import_entry;
    while (true)
    {
        import_entry = stream.read<import_entry_t>();
        if (memcmp(&import_entry, &import_entry_null, sizeof(import_entry_t)) == 0)
        {
            std::wcerr << L"Error: Input file does not import ddraw.dll.\n";
            return false;
        }

        auto import_entry_position = seekable.position();
        seekable.seek(stdext::seek_from::begin, section_header.raw_data_offset + import_entry.dllname_virtual_address - idata_base_rva);
        auto import_name_position = seekable.position();
        std::string dll_name;
        for (char ch; (ch = stream.read<char>()) != '\0'; )
            dll_name.push_back(ch);
        if (_stricmp(dll_name.c_str(), "ddraw.dll") == 0)
        {
            seekable.set_position(import_name_position);
            const char import_name[] = "wcdx.dll";
            stream.write_all(import_name);
            break;
        }

        seekable.set_position(import_entry_position);
    }

    seekable.seek(stdext::seek_from::begin, section_header.raw_data_offset + import_entry.lookup_virtual_address - idata_base_rva);
    if (pe_type_signature != OptionalHeader_PE32Signature)
    {
        std::wcerr << L"Error: Input file missing PE32 signature.\n";
        return false;
    }

    stdext::stream_position lookup_position = 0;
    while (true)
    {
        auto lookup = stream.read<uint32_t>();
        if (lookup == 0)
        {
            std::wcerr << L"Error: Input file does not import DirectDrawCreate.\n";
            return false;
        }

        if ((lookup & 0x80000000) == 0)
        {
            lookup_position = seekable.position();
            seekable.seek(stdext::seek_from::begin, section_header.raw_data_offset + lookup - idata_base_rva + 2);
            auto name_position = seekable.position();

            std::string name;
            for (char ch; (ch = stream.read<char>()) != '\0'; )
                name += ch;
            if (name == "DirectDrawCreate")
            {
                seekable.set_position(name_position - 2);
                break;
            }
            seekable.set_position(lookup_position);
        }
    }

    assert(lookup_position != 0);

    stream.write(uint16_t(0));
    const char function_name[] = "WcdxCreate";
    stream.write_all(function_name);

    seekable.set_position(lookup_position);
    stream.write(0);
    return true;
}

bool apply_dif(stdext::multi_ref<stdext::stream, stdext::seekable> file_data, uint32_t hash)
{
    static const std::map<uint32_t, uint32_t> diffs =
    {
        { 0x8c99fb40, RESOURCE_ID_WING1_DIFF },
        { 0xfce65eac, RESOURCE_ID_TRANSFER_DIFF },
        { 0xa6ddc22a, RESOURCE_ID_SM1_DIFF },
        { 0x74350efd, RESOURCE_ID_SM2_DIFF },
        { 0x067a8af5, RESOURCE_ID_WING2_DIFF }
    };

    auto i = diffs.find(hash);
    if (i == diffs.end())
        return false;

    resource_stream resource(i->second);
    auto line = read_line(resource);
    char tag[] = "This difference file has been created by IDA";
    if (line == std::nullopt || mismatch(line->begin(), line->end(), std::begin(tag), std::end(tag)).second != std::end(tag) - 1)
        return false;

    while ((line = read_line(resource)) != std::nullopt)
    {
        if (line->empty())
            continue;

        auto n = line->find(':');
        if (n == std::string::npos)
            continue;
        stdext::string_view address_str(line->data(), n);

        n = line->find_first_not_of(' ', n + 1);
        if (n == std::string::npos)
            return false;
        auto m = line->find(' ', n);
        if (n == std::string::npos)
            return false;
        stdext::string_view original_value_str(line->data() + n, m - n);

        n = line->find_first_not_of(' ', m + 1);
        if (n == std::string::npos)
            return false;
        m = line->find(' ', n);
        if (m == std::string::npos)
            m = line->length();
        stdext::string_view replacement_value_str(line->data() + n, m - n);

        if (line->find_first_not_of(' ', m + 1) != std::string::npos)
            return false;

        if (original_value_str.compare("FFFFFFFF") == 0)
            continue;

        uint32_t offset = stoul(address_str, nullptr, 16);
        auto original_value = std::byte(stoul(original_value_str, nullptr, 16));
        auto replacement_value = std::byte(stoul(replacement_value_str, nullptr, 16));

        auto& stream = file_data.as<stdext::stream>();
        auto& seekable = file_data.as<stdext::seekable>();
        seekable.seek(stdext::seek_from::begin, offset);
        auto value = stream.read<std::byte>();
        if (value != original_value)
            return false;
        seekable.seek(stdext::seek_from::current, -1);
        stream.write(replacement_value);
    }

    return true;
}

std::optional<std::string> read_line(stdext::multi_ref<stdext::input_stream, stdext::seekable> is)
{
    auto& stream = is.as<stdext::input_stream>();
    auto& seekable = is.as<stdext::seekable>();

    char ch;
    if (stream.read(&ch, 1) == 0)
        return std::nullopt;

    seekable.seek(stdext::seek_from::current, -1);
    auto position = seekable.position();
    size_t length = 0;
    while (stream.read(&ch, 1) == 1 && (ch != '\r') && (ch != '\n'))
        ++length;
    seekable.set_position(position);

    std::string line;
    line.reserve(length);
    while (stream.read(&ch, 1) == 1 && (ch != '\r') && (ch != '\n'))
        line += ch;

    if ((ch == '\r') && stream.read(&ch, 1) == 1 && (ch != '\n'))
        seekable.seek(stdext::seek_from::current, -1);

    return line;
}

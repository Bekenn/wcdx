#include <stdext/file.h>
#include <stdext/string.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <cstdint>
#include <cstdlib>
#include <cwchar>


namespace
{
    enum : uint32_t
    {
        mode_none           = 0x0,
        mode_extract        = 0x1,
        mode_extract_all    = 0x2,
        mode_pack           = 0x4,
        mode_replace        = 0x8,

        mode_operation_mask = mode_extract | mode_extract_all | mode_pack | mode_replace
    };

    struct program_options
    {
        uint32_t mode = mode_none;
        std::vector<const wchar_t*> input_paths;
        const wchar_t* output_path = nullptr;
        unsigned index = unsigned(-1);
    };

    class usage_error : public std::runtime_error
    {
        using runtime_error::runtime_error;
    };

    void show_usage(const wchar_t* invocation);
    void diagnose_options(const program_options& options);

    void extract_all(stdext::file_input_stream& input_file, const wchar_t* output_path);
    void extract_one(stdext::file_input_stream& input_file, unsigned index, const wchar_t* output_path);
}

int wmain(int argc, wchar_t* argv[])
{
    std::wstring invocation = argc > 0 ? std::filesystem::path(argv[0]).filename() : "wcres";

    try
    {
        if (argc == 1)
        {
            show_usage(invocation.c_str());
            return EXIT_SUCCESS;
        }

        program_options options;

        for (int n = 1; n < argc; ++n)
        {
            if (argv[n][0] == L'-')
            {
                if (wcscmp(argv[n], L"-extract") == 0)
                {
                    if ((options.mode & mode_extract) != 0)
                        throw usage_error("The -extract option can only be used once.");

                    options.mode |= mode_extract;
                    if (++n != argc)
                    {
                        wchar_t* endp;
                        options.index = unsigned(wcstoul(argv[n], &endp, 10));
                        if (*endp != L'\0')
                            throw usage_error("Bad resource index: " + stdext::to_mbstring(argv[n]));
                    }

                    diagnose_options(options);
                }
                else if (wcscmp(argv[n], L"-extract-all") == 0)
                {
                    if ((options.mode & mode_extract_all) != 0)
                        throw usage_error("The -extract-all option can only be used once.");

                    options.mode |= mode_extract_all;
                    diagnose_options(options);
                }
                else if (wcscmp(argv[n], L"-pack") == 0)
                {
                    if ((options.mode & mode_pack) != 0)
                        throw usage_error("The -pack option can only be used once.");

                    options.mode |= mode_pack;
                    diagnose_options(options);
                }
                else if (wcscmp(argv[n], L"-replace") == 0)
                {
                    if ((options.mode & mode_replace) != 0)
                        throw usage_error("The -replace option can only be used once.");

                    options.mode |= mode_replace;
                    diagnose_options(options);
                }
                else if (wcscmp(argv[n], L"-o") == 0)
                {
                    if (options.output_path != nullptr)
                        throw usage_error("Only one output path can be specified.");

                    if (++n == argc)
                        throw usage_error("Missing output path");

                    options.output_path = argv[n];
                    diagnose_options(options);
                }
            }
            else
            {
                if (options.input_paths.size() > 0 && (options.mode & mode_pack) != mode_pack)
                    throw usage_error("Unrecognized argument: " + stdext::to_mbstring(argv[n]));
                options.input_paths.push_back(argv[n]);
            }
        }

        if (options.input_paths.size() == 0)
            throw usage_error("No input path specified");

        stdext::file_input_stream input_file(options.input_paths.front());
        switch (options.mode & mode_operation_mask)
        {
        case mode_extract:
            extract_one(input_file, options.index, options.output_path);
            break;

        case mode_extract_all:
            extract_all(input_file, options.output_path);
            break;

        case mode_pack:
        case mode_replace:
            break;

        default:
            throw usage_error("No command option specified");
        }

        return EXIT_SUCCESS;
    }
    catch (const usage_error& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        show_usage(invocation.c_str());
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Unknown error" << '\n';
    }

    return EXIT_FAILURE;
}

namespace
{
    void show_usage(const wchar_t* invocation)
    {
        std::wcout <<
            L"Usage: " << invocation << " -o <output_path> -extract <resource_index> <input_path>\n"
            L"       " << invocation << " -o <output_path> -extract-all <input_path>\n"
            L"       " << invocation << " -o <output_path> -pack <input_path>...\n"
            L"       " << invocation << " -o <output_path> -replace <resource_index> <input_path>\n"
            L"\n"
            L"With -extract or -extract-all, extracts resources from files found in the\n"
            L"GAMEDAT folder of wc1 and wc2.  With -pack, creates a new archive from the given\n"
            L"input files.\n"
            L"\n"
            L"The -extract option extracts a single resource from a file and saves it at\n"
            L"<output_path>.  Resources in a file are numbered starting from 0, with the\n"
            L"resource number given as <resource_index>.\n"
            L"\n"
            L"The -extract-all option extracts all resources from a file, saving them in a\n"
            L"directory at <output_path>.  If <output_path> does not exist, a new directory\n"
            L"will be created.  Resources are saved in files named with the resource number.\n"
            L"\n"
            L"The -pack option is the opposite of the -extract-all option.  Instead of\n"
            L"extracting resources from an archive, the -pack option creates a new archive\n"
            L"at <output_path> from the given <input_path> arguments.  Any number of\n"
            L"<input_path>s may be given.  Resources will be packed in the same order as they\n"
            L"appear on the command line.\n";
    }

    void diagnose_options(const program_options& options)
    {
        stdext::discard(options);
    }

    void extract_all(stdext::file_input_stream& input_file, const wchar_t* output_path)
    {
        [[maybe_unused]] auto file_size = input_file.read<uint32_t>();
        auto first_resource_offset = input_file.read<uint32_t>() & 0x00FFFFFF;
        auto resource_count = (first_resource_offset - 4) / 4;

        auto dir = std::filesystem::current_path();
        if (output_path == nullptr)
            output_path = dir.c_str();

        std::filesystem::create_directories(output_path);
        for (uint32_t n = 0; n < resource_count; ++n)
        {
            input_file.set_position(0);
            extract_one(input_file, n, (std::filesystem::path(output_path) /= std::to_wstring(n)).c_str());
        }
    }

    void extract_one(stdext::file_input_stream& input_file, unsigned index, const wchar_t* output_path)
    {
        auto descriptor_offset = 4 + (4 * index);
        auto file_size = input_file.read<uint32_t>();
        auto first_resource_offset = input_file.read<uint32_t>() & 0x00FFFFFF;
        if (first_resource_offset <= descriptor_offset)
            throw std::range_error("Resource index " + std::to_string(index) + " out of range");

        input_file.set_position(descriptor_offset);
        auto resource_offset = input_file.read<uint32_t>() & 0x00FFFFFF;
        auto resource_size = input_file.position() == first_resource_offset
            ? file_size - resource_offset
            : (input_file.read<uint32_t>() & 0x00FFFFFF) - resource_offset;

        input_file.set_position(resource_offset);

        std::byte buf[0x1000];
        stdext::file_output_stream output_file(output_path);
        while (resource_size != 0)
        {
            auto bytes = input_file.read(buf, std::min(size_t(resource_size), stdext::lengthof(buf)));
            output_file.write_all(buf, bytes);
            resource_size -= uint32_t(bytes);
        }
    }
}

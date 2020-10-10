#include <image/image.h>
#include <image/resources.h>

#include <stdext/array_view.h>
#include <stdext/file.h>
#include <stdext/format.h>
#include <stdext/multi.h>
#include <stdext/utility.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>

#include <cassert>
#include <cstddef>
#include <cstdlib>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <comdef.h>


namespace
{
    struct glyph_info
    {
        uint16_t file_position;
        uint8_t width;
        uint8_t height;
        std::unique_ptr<std::byte[]> pixels;
    };

    enum : uint32_t
    {
        mode_none               = 0x0,
        mode_extract_glyph      = 0x1,
        mode_extract_all_glyphs = 0x2,
        mode_extract_font_strip = 0x4
    };

    struct program_options
    {
        uint32_t mode = mode_none;
        const wchar_t* input_path = nullptr;
        const wchar_t* output_path = nullptr;
        const wchar_t* prefix = nullptr;
        unsigned glyph_index = unsigned(-1);
    };

    class usage_error : public std::runtime_error
    {
        using runtime_error::runtime_error;
    };

    program_options parse_args(int argc, wchar_t* argv[]);
    void show_usage(const wchar_t* invocation);
    void diagnose_options(const program_options& options);

    using glyph_array = std::array<glyph_info, 0x100>;
    glyph_array read_font(stdext::multi_ref<stdext::input_stream, stdext::seekable> input);
    void extract_glyph(const glyph_info& glyph, stdext::array_view<std::byte> palette_view, const wchar_t* output_path);
    void extract_all_glyphs(const glyph_array& glyphs, stdext::array_view<std::byte> palette_view, const wchar_t* output_path, const wchar_t* prefix);
    void extract_font_strip(const glyph_array& glyphs, stdext::array_view<std::byte> palette_view, const wchar_t* output_path);
}

int wmain(int argc, wchar_t* argv[])
{
    std::wstring invocation = argc > 0 ? std::filesystem::path(argv[0]).filename() : "wc2font";

    try
    {
        if (argc == 1)
        {
            show_usage(invocation.c_str());
            return EXIT_SUCCESS;
        }

        auto options = parse_args(argc, argv);

        auto palette_resource = ::FindResource(nullptr, MAKEINTRESOURCE(RESOURCE_ID_WC2PAL), RT_RCDATA);
        auto palette_hglobal = ::LoadResource(nullptr, palette_resource);
        auto palette_data = ::LockResource(palette_hglobal);
        auto palette_size = ::SizeofResource(nullptr, palette_resource);
        stdext::array_view<std::byte> palette_view(static_cast<std::byte*>(palette_data), palette_size);

        stdext::file_input_stream file(options.input_path);
        auto glyphs = read_font(file);

        switch (options.mode)
        {
        case mode_extract_glyph:
            extract_glyph(glyphs[options.glyph_index], palette_view, options.output_path);
            break;
        case mode_extract_all_glyphs:
            extract_all_glyphs(glyphs, palette_view, options.output_path, options.prefix);
            break;
        case mode_extract_font_strip:
            extract_font_strip(glyphs, palette_view, options.output_path);
            break;
        default:
            stdext::unreachable();
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
        stdext::format(stdext::strerr(), "Error: $0\n", e.what());
    }
    catch (const _com_error& e)
    {
        std::wcerr << L"Error: " << e.ErrorMessage() << L'\n';
    }
    catch (...)
    {
        stdext::format(stdext::strerr(), "Unknown error\n");
    }

    return EXIT_FAILURE;
}

namespace
{
    program_options parse_args(int argc, wchar_t* argv[])
    {
        program_options options;

        for (int n = 1; n < argc; ++n)
        {
            if (argv[n][0] == L'-')
            {
                if (wcscmp(argv[n], L"-o") == 0)
                {
                    if (++n == argc)
                        throw usage_error("Expected output path for -o");
                    if (options.output_path != nullptr)
                        throw usage_error("The -o option can only be used once");
                    options.output_path = argv[n];
                }
                else if (wcscmp(argv[n], L"-extract-glyph") == 0)
                {
                    if ((options.mode & mode_extract_glyph) != 0)
                        throw usage_error("The -extract-glyph option can only be used once");

                    options.mode |= mode_extract_glyph;
                    if (++n == argc)
                        throw usage_error("Missing glyph index");

                    wchar_t* endp;
                    options.glyph_index = unsigned(wcstoul(argv[n], &endp, 10));
                    if (*endp != L'\0')
                        throw usage_error("Bad glyph index: " + stdext::to_mbstring(argv[n]));

                    diagnose_options(options);
                }
                else if (wcscmp(argv[n], L"-extract-all-glyphs") == 0)
                {
                    if ((options.mode & mode_extract_all_glyphs) != 0)
                        throw usage_error("The -extract-all-glyphs option can only be used once");

                    options.mode |= mode_extract_all_glyphs;
                    diagnose_options(options);
                }
                else if (wcscmp(argv[n], L"-prefix") == 0)
                {
                    if (++n == argc)
                        throw usage_error("Expected prefix");
                    if (options.prefix != nullptr)
                        throw usage_error("The -prefix option can only be used once");
                    options.prefix = argv[n];
                    diagnose_options(options);
                }
                else if (wcscmp(argv[n], L"-extract-font-strip") == 0)
                {
                    if ((options.mode & mode_extract_font_strip) != 0)
                        throw usage_error("The -extract-font-strip option can only be used once");

                    options.mode |= mode_extract_font_strip;
                    diagnose_options(options);
                }
                else
                    throw usage_error("Unrecognized option: " + stdext::to_mbstring(argv[n]));
            }
            else
            {
                if (options.input_path != nullptr)
                    throw usage_error("Unexpected argument: " + stdext::to_mbstring(argv[n]));
                options.input_path = argv[n];
            }
        }

        if (options.input_path == nullptr)
            throw usage_error("Missing input path");
        if (options.output_path == nullptr)
            throw usage_error("Missing output path");
        if (options.mode == 0)
            throw usage_error("Missing -extract-glyph, -extract-all-glyphs, or -extract-font-strip");
        if (options.prefix == nullptr)
            options.prefix = L"";

        return options;
    }

    void show_usage(const wchar_t* invocation)
    {
        std::wcout << L"Usage:\n"
            L"    " << invocation << " -o <output_path> -extract-glyph <glyph_index> <input_path>\n"
            L"    " << invocation << " -o <output_path> -extract-all-glyphs [-prefix <name_prefix>] <input_path>\n"
            L"    " << invocation << " -o <output_path> -extract-font-strip <input_path>\n"
            L"\n"
            L"input_path is an extracted font resource for Wing Commander II.  You can get it\n"
            L"by running wcres against fonts.fnt.\n"
            L"\n"
            L"output_path points to a location where data will be written out.  For\n"
            L"-extract-glyph and -extract-font-strip, this should name a file ending in .png.\n"
            L"For -extract-all-glyphs, this should name a directory.\n"
            L"\n"
            L"The -extract-glyph option extracts a single glyph from the font resource, saving\n"
            L"it as a PNG-encoded image file.  Note that zero-sized glyphs cannot be\n"
            L"extracted.\n"
            L"\n"
            L"The -extract-all-glyphs option extracts all non-zero-sized glyphs from the font\n"
            L"resource, saving them as PNG-encoded image files.  Files are named according to\n"
            L"the index of the corresponding glyph, with an optional prefix."
            L"\n"
            L"The -extract-font-strip option extracts all glyphs from the font resource,\n"
            L"concatenating them into a single image.\n"
            L"\n"
            L"glyph_index is the numeric value of a character in the font.  It can be any\n"
            L"value from 0 to 255, and typically corresponds with the ASCII encoding of the\n"
            L"character.\n"
            L"\n"
            L"name_prefix is a string that will be prepended to the names of the files that\n"
            L"will be written to the output directory for -extract-all-glyphs.\n";
    }

    void diagnose_options(const program_options& options)
    {
        if ((options.mode & mode_extract_glyph) != 0)
        {
            if ((options.mode & mode_extract_all_glyphs) != 0)
                throw usage_error("The -extract-glyph option cannot be used with -extract-all-glyphs");
            if ((options.mode & mode_extract_font_strip) != 0)
                throw usage_error("The -extract-glyph option cannot be used with -extract-file-strip");
            if (options.prefix != nullptr)
                throw usage_error("The -prefix option cannot be used with -extract-glyph");
            if (options.glyph_index == unsigned(-1))
                throw usage_error("Missing glyph index for -extract-glyph");
        }
        if ((options.mode & mode_extract_all_glyphs) != 0)
        {
            if ((options.mode & mode_extract_font_strip) != 0)
                throw usage_error("The -extract-all-glyphs option cannot be used with -extract-font-strip");
        }
        if ((options.mode & mode_extract_font_strip) != 0)
        {
            if (options.prefix)
                throw usage_error("The -prefix option cannot be used with -extract-font-strip");
        }
    }

    std::array<glyph_info, 0x100> read_font(stdext::multi_ref<stdext::input_stream, stdext::seekable> input)
    {
        auto& stream = input.as<stdext::input_stream>();
        auto& seeker = input.as<stdext::seekable>();

        auto font_height = stream.read<uint16_t>();
        stream.skip<uint16_t>();    // color index, duplicative of pixel data

        std::array<glyph_info, 0x100> glyphs;

        for (auto& glyph : glyphs)
        {
            glyph.width = stream.read<uint8_t>();
            glyph.height = uint8_t(font_height);
        }

        for (auto& glyph : glyphs)
            glyph.file_position = stream.read<uint8_t>();

        for (auto& glyph : glyphs)
            glyph.file_position |= uint16_t(stream.read<uint8_t>()) << 8;

        for (auto& glyph : glyphs)
        {
            assert(seeker.position() == glyph.file_position);
            size_t pixel_count = glyph.width * glyph.height;
            if (pixel_count == 0)
                continue;

            glyph.pixels = std::make_unique<std::byte[]>(pixel_count);
            stream.read_all(glyph.pixels.get(), pixel_count);
        }

        assert(seeker.position() == seeker.end_position());
        stdext::discard(seeker);

        return glyphs;
    }

    void extract_glyph(const glyph_info& glyph, stdext::array_view<std::byte> palette_view, const wchar_t* output_path)
    {
        stdext::memory_input_stream pixels_stream(glyph.pixels.get(), glyph.width * glyph.height);
        stdext::file_output_stream out(output_path);
        wcdx::image::write_image({ glyph.width, glyph.height }, palette_view, pixels_stream, out);
    }

    void extract_all_glyphs(const glyph_array& glyphs, stdext::array_view<std::byte> palette_view, const wchar_t* output_path, const wchar_t* prefix)
    {
        size_t index = 0;
        for (auto& glyph : glyphs)
        {
            if (glyph.width == 0 || glyph.height == 0)
            {
                ++index;
                continue;
            }

            stdext::memory_input_stream pixels_stream(glyph.pixels.get(), glyph.width * glyph.height);
            stdext::file_output_stream out(stdext::format_string("$0\\$1$2.png", output_path, prefix, index++).c_str(), stdext::utf8_path_encoding());
            wcdx::image::write_image({ glyph.width, glyph.height }, palette_view, pixels_stream, out);
        }
    }

    void extract_font_strip(const glyph_array& glyphs, stdext::array_view<std::byte> palette_view, const wchar_t* output_path)
    {
        unsigned width = std::accumulate(glyphs.begin(), glyphs.end(), unsigned(0), [](unsigned acc, const glyph_info& glyph)
        {
            return acc + glyph.width;
        });
        unsigned height = glyphs[0].height;

        auto pixels = std::make_unique<std::byte[]>(width * height);
        auto p = pixels.get();
        for (auto& glyph : glyphs)
        {
            assert(p + glyph.width <= pixels.get() + width);
            auto src = glyph.pixels.get();
            auto dst = p;
            for (unsigned y = 0; y < glyph.height; ++y)
            {
                memcpy(dst, src, glyph.width);
                dst += width;
                src += glyph.width;
            }

            p += glyph.width;
        }

        stdext::memory_input_stream pixels_stream(pixels.get(), width * height);
        stdext::file_output_stream out(output_path);
        wcdx::image::write_image({ width, height }, palette_view, pixels_stream, out);
    }
}

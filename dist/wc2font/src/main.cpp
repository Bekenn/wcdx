#include <image/image.h>
#include <image/resources.h>

#include <stdext/array_view.h>
#include <stdext/file.h>
#include <stdext/format.h>
#include <stdext/multi.h>
#include <stdext/utility.h>

#include <array>
#include <iostream>
#include <memory>
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

    std::array<glyph_info, 0x100> read_font(stdext::multi_ref<stdext::input_stream, stdext::seekable> input);
}

int wmain()
{
    try
    {
        auto palette_resource = ::FindResource(nullptr, MAKEINTRESOURCE(RESOURCE_ID_WC2PAL), RT_RCDATA);
        auto palette_hglobal = ::LoadResource(nullptr, palette_resource);
        auto palette_data = ::LockResource(palette_hglobal);
        auto palette_size = ::SizeofResource(nullptr, palette_resource);

        stdext::file_input_stream file(L"Z:\\disasm\\Kilrathi Saga\\wc2\\fonts.fnt\\2");
        auto glyphs = read_font(file);
        size_t index = 0;
        for (auto& glyph : glyphs)
        {
            if (glyph.width == 0 || glyph.height == 0)
            {
                ++index;
                continue;
            }

            stdext::array_view<std::byte> palette_view(static_cast<std::byte*>(palette_data), palette_size);
            stdext::memory_input_stream pixels_stream(glyph.pixels.get(), glyph.width * glyph.height);
            stdext::file_output_stream out(stdext::format_string("Z:\\disasm\\Kilrathi Saga\\wc2\\fonts.fnt\\2_$0.png", index++).c_str(), stdext::utf8_path_encoding());
            wcdx::image::write_image({ glyph.width, glyph.height }, palette_view, pixels_stream, out);
        }

        return EXIT_SUCCESS;
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

        return glyphs;
    }
}

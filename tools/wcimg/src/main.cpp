#include "../res/resources.h"

#include <stdext/file.h>
#include <stdext/multi.h>
#include <stdext/string.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <iostream>

#include <cstdlib>
#include <cstdint>
#include <cwchar>

#define NOMINMAX
#include <Windows.h>
#include <comip.h>
#include <comdef.h>
#include <wincodec.h>


_COM_SMARTPTR_TYPEDEF(IWICImagingFactory, __uuidof(IWICImagingFactory));
_COM_SMARTPTR_TYPEDEF(IWICBitmap, __uuidof(IWICBitmap));
_COM_SMARTPTR_TYPEDEF(IWICPalette, __uuidof(IWICPalette));
_COM_SMARTPTR_TYPEDEF(IWICBitmapLock, __uuidof(IWICBitmapLock));
_COM_SMARTPTR_TYPEDEF(IWICStream, __uuidof(IWICStream));
_COM_SMARTPTR_TYPEDEF(IWICBitmapEncoder, __uuidof(IWICBitmapEncoder));
_COM_SMARTPTR_TYPEDEF(IWICBitmapFrameEncode, __uuidof(IWICBitmapFrameEncode));
_COM_SMARTPTR_TYPEDEF(IWICBitmapDecoder, __uuidof(IWICBitmapDecoder));
_COM_SMARTPTR_TYPEDEF(IWICBitmapFrameDecode, __uuidof(IWICBitmapFrameDecode));
_COM_SMARTPTR_TYPEDEF(IWICFormatConverter, __uuidof(IWICFormatConverter));

#define COM_REQUIRE_SUCCESS(expr) if (FAILED(hr = (expr))) _com_raise_error(hr)

enum class game_id
{
    wc1,
    wc2
};

struct point
{
	int16_t x, y;
};

struct rect
{
	point p1, p2;
};

class usage_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

static void show_usage(const wchar_t* invocation);

static void extract_images(stdext::multi_ref<stdext::input_stream, stdext::seekable> input, game_id game, const wchar_t* output_path, const wchar_t* prefix);
static void extract_image(stdext::multi_ref<stdext::input_stream, stdext::seekable> input, game_id game, int index, const wchar_t* output_path);
static void extract_image(stdext::multi_ref<stdext::input_stream, stdext::seekable> input, game_id game, const wchar_t* output_path);
static void pack_images(const std::vector<const wchar_t*>& input_paths, game_id game, const std::vector<point>& reference_points, const wchar_t* output_path);
static void pack_image(const IWICImagingFactoryPtr& imaging_factory, const IWICPalettePtr& palette, const IWICBitmapFrameDecodePtr& input, point reference_point, stdext::output_stream& output);
static rect get_image_dimensions(stdext::multi_ref<stdext::input_stream, stdext::seekable> input);

int wmain(int argc, wchar_t* argv[])
{
    std::wstring invocation = argc > 0 ? std::filesystem::path(argv[0]).filename() : "wcimg";
    
    try
	{
        if (argc < 2)
        {
            show_usage(invocation.c_str());
            return EXIT_SUCCESS;
        }

        std::vector<const wchar_t*> input_paths;
        std::vector<point> reference_points;
        auto game = game_id::wc1;
		const wchar_t* output_path = nullptr;
        const wchar_t* output_prefix = nullptr;
		int index = -1;

        enum class mode { unspecified, extract, extract_all, pack } invocation_mode = mode::unspecified;

        for (int n = 1; n < argc; ++n)
		{
			if (argv[n][0] == L'-')
			{
                if (wcscmp(argv[n], L"-wc1") == 0)
                    game = game_id::wc1;
                else if (wcscmp(argv[n], L"-wc2") == 0)
                    game = game_id::wc2;
				else if (wcscmp(argv[n], L"-extract") == 0)
				{
                    if (invocation_mode != mode::unspecified)
                        throw usage_error("Only one of -extract, -extract_all, or -pack may be specified");
                    invocation_mode = mode::extract;

                    if (++n == argc)
						throw usage_error("No index for -extract");

					index = _wtoi(argv[n]);
					if (index < 0)
						throw usage_error("Index for -extract must be non-negative");
				}
                else if (wcscmp(argv[n], L"-extract-all") == 0)
                {
                    if (invocation_mode != mode::unspecified)
                        throw usage_error("Only one of -extract, -extract_all, or -pack may be specified");
                    invocation_mode = mode::extract_all;
                }
                else if (wcscmp(argv[n], L"-pack") == 0)
                {
                    if (invocation_mode != mode::unspecified)
                        throw usage_error("Only one of -extract, -extract_all, or -pack may be specified");
                    invocation_mode = mode::pack;
                }
                else if (wcscmp(argv[n], L"-prefix") == 0)
                {
                    if (++n == argc)
                        throw usage_error("No value for -prefix");

                    output_prefix = argv[n];
                }
                else if (wcscmp(argv[n], L"-ref") == 0)
                {
                    if (invocation_mode != mode::pack)
                        throw usage_error("-pack must precede -ref");
                    if (reference_points.size() == 0)
                        throw usage_error("-ref must follow an image argument");

                    wchar_t* p;

                    if (++n == argc)
                        throw usage_error("No x value for -ref");
                    auto x = int16_t(wcstol(argv[n], &p, 10));
                    if (*p != L'\0')
                        throw usage_error("Bad ref value");

                    if (++n == argc)
                        throw usage_error("No y value for -ref");
                    auto y = int16_t(wcstol(argv[n], &p, 10));
                    if (*p != L'\0')
                        throw usage_error("Bad ref value");

                    reference_points.back() = { x, y };
                }
				else if (wcscmp(argv[n], L"-o") == 0)
				{
					if (++n == argc)
						throw usage_error("No output file specified for -o");

					output_path = argv[n];
				}
				else
					throw usage_error("Unrecognized option " + stdext::to_mbstring(argv[n]));
			}
			else
            {
				input_paths.push_back(argv[n]);
                if (invocation_mode == mode::pack)
                    reference_points.push_back({ 0, 0 });
            }
		}

		if (input_paths.size() == 0)
			throw usage_error("No input file specified");

        if (invocation_mode != mode::pack && input_paths.size() > 1)
            throw usage_error("Multiple input files specified");

        switch (invocation_mode)
        {
        case mode::extract:
            extract_image(stdext::file_input_stream(input_paths.front()), game, index, output_path);
            break;
        case mode::extract_all:
            extract_images(stdext::file_input_stream(input_paths.front()), game, output_path, output_prefix);
            break;
        case mode::pack:
            pack_images(input_paths, game, reference_points, output_path);
            break;

        default:
            throw usage_error("At least one of -extract, -extract_all, and -pack must be specified");
        }

		return EXIT_SUCCESS;
	}
    catch (const usage_error& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        show_usage(invocation.c_str());
    }
    catch (const _com_error& e)
	{
		std::wcerr << L"Error: " << e.ErrorMessage() << L'\n';
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

void show_usage(const wchar_t* invocation)
{
    std::wcout << L"Usage:\n"
        L"    " << invocation << L" -o <output_path> [-wc1 | -wc2] -extract <image_index> <input_path>\n"
        L"    " << invocation << L" -o <output_path> [-wc1 | -wc2] -extract-all -prefix <name_prefix> <input_path>\n"
        L"    " << invocation << L" -o <output_path> [-wc1 | -wc2] -pack <input_path> [-ref <x> <y>] ...\n"
        L"\n"
        L"image_index gives the zero-based index of the image to be extracted.\n"
        L"\n"
        L"When using the -extract-all option, output_path specifies a directory instead\n"
        L"of a file name.  A new file is created for each image in the input file.  File\n"
        L"names begin with 0.png, with each succeeding file name incrementing the number\n"
        L"by one.  If the -prefix option is given, then the specified sequence of\n"
        L"characters is prepended to each file name.\n"
        L"\n"
        L"Example:\n"
        L"    " << invocation << L" -o images -extract-all -prefix foo imageset\n"
        L"This invocation will output several files in the images directory with names of\n"
        L"the form foo<n>.png, where <n> gives the numeric index of each image.\n"
        L"\n"
        L"The -wc1 and -wc2 options are used to select a color palette appropriate to a\n"
        L"given game.  If neither is specified, -wc1 is assumed.\n"
        L"\n"
        L"The -pack option accepts any number of input files.  Each must be an image file.\n"
        L"The images are converted and packed into an image set of the format expected for\n"
        L"Wing Commander image resources.  Colors are converted to the palette used by\n"
        L"Wing Commander 1.\n"
        L"\n"
        L"When the -pack option is specified, each image may be followed by a -ref\n"
        L"argument giving the coordinates of the image's reference point.  The reference\n"
        L"point represents the logical center of the image for rotation, scaling, and\n"
        L"drawing purposes.  If a reference point is not specified for an image, then it\n"
        L"is as if -ref 0 0 had been specified.\n";
}

void extract_images(stdext::multi_ref<stdext::input_stream, stdext::seekable> input, game_id game, const wchar_t* output_path, const wchar_t* prefix)
{
    auto cwd = std::filesystem::current_path();
    if (output_path == nullptr)
        output_path = cwd.c_str();
    if (prefix == nullptr)
        prefix = L"";

    auto& stream = input.as<stdext::input_stream>();
    auto& seeker = input.as<stdext::seekable>();

    auto begin_pos = seeker.position();
    auto file_size = stream.read<uint32_t>();
    auto first_offset = stream.read<uint32_t>();
    if (first_offset % 4 != 0)
        throw std::runtime_error("Input file is not an image archive");

    auto image_count = (first_offset - 4) / 4;
    for (unsigned n = 0; n < image_count; ++n)
    {
        seeker.set_position(begin_pos + 4 + 4 * n);
        auto image_offset = stream.read<uint32_t>();
        if (image_offset >= file_size)
            throw std::runtime_error("Bad image offset");

        seeker.set_position(begin_pos + image_offset);
        extract_image(input, game, (std::filesystem::path(output_path) /= prefix + std::to_wstring(n) + L".png").c_str());
    }
}

void extract_image(stdext::multi_ref<stdext::input_stream, stdext::seekable> input, game_id game, int index, const wchar_t* output_path)
{
    auto& stream = input.as<stdext::input_stream>();
    auto& seeker = input.as<stdext::seekable>();

    if (output_path == nullptr)
        throw std::runtime_error("No output file specified");

    auto size = stream.read<uint32_t>();
    auto header_offset = unsigned(4 + 4 * index);
    if (size <= header_offset)
        throw std::runtime_error("Invalid index");

    auto first_image_offset = stream.read<uint32_t>();
    if (first_image_offset <= header_offset)
        throw std::runtime_error("Invalid index");

    seeker.set_position(header_offset);
    auto image_offset = stream.read<uint32_t>();

    seeker.set_position(image_offset);
    extract_image(input, game, output_path);
}

void extract_image(stdext::multi_ref<stdext::input_stream, stdext::seekable> input, game_id game, const wchar_t* output_path)
{
    auto& stream = input.as<stdext::input_stream>();
    auto& seeker = input.as<stdext::seekable>();
	auto start_position = seeker.position();
	auto dimensions = get_image_dimensions(input);

    WICColor colors[256];

    WORD resid = game == game_id::wc1 ? RESOURCE_ID_WC1PAL : RESOURCE_ID_WC2PAL;
    size_t offset = game == game_id::wc1 ? 0x30 : 0;

    auto res = ::FindResource(nullptr, MAKEINTRESOURCE(resid), L"BINARY");
    if (res == nullptr)
        throw std::system_error(::GetLastError(), std::system_category());
    auto resp = ::LoadResource(nullptr, res);
    auto palette_data = ::LockResource(resp);

	auto palette_p = static_cast<const BYTE*>(palette_data) + offset;
	auto color_p = colors;
	for (size_t n = 0; n < stdext::lengthof(colors); ++n)
	{
		auto red = *palette_p++;
		auto green = *palette_p++;
		auto blue = *palette_p++;
		*color_p++ = blue | (green << 8) | (red << 16) | (0xFF << 24);
	}
    colors[stdext::lengthof(colors) - 1] &= 0x00FFFFFF; // last entry transparent

	HRESULT hr;
    COM_REQUIRE_SUCCESS(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
	at_scope_exit(::CoUninitialize);

	IWICImagingFactoryPtr imaging_factory;
	COM_REQUIRE_SUCCESS(imaging_factory.CreateInstance(CLSID_WICImagingFactory));

	IWICPalettePtr palette;
	COM_REQUIRE_SUCCESS(imaging_factory->CreatePalette(&palette));
	COM_REQUIRE_SUCCESS(palette->InitializeCustom(colors, stdext::lengthof(colors)));

	IWICBitmapPtr bitmap;
	COM_REQUIRE_SUCCESS(imaging_factory->CreateBitmap(dimensions.p2.x - dimensions.p1.x,
                                                      dimensions.p2.y - dimensions.p1.y,
                                                      GUID_WICPixelFormat8bppIndexed,
                                                      WICBitmapCacheOnDemand,
                                                      &bitmap));
	COM_REQUIRE_SUCCESS(bitmap->SetPalette(palette));

	WICRect lock_rect =
	{
		0, 0,
		dimensions.p2.x - dimensions.p1.x, dimensions.p2.y - dimensions.p1.y
	};

	IWICBitmapLockPtr lock;
	COM_REQUIRE_SUCCESS(bitmap->Lock(&lock_rect, WICBitmapLockWrite, &lock));

	UINT buffer_size;
	WICInProcPointer bitmap_data;
	COM_REQUIRE_SUCCESS(lock->GetDataPointer(&buffer_size, &bitmap_data));

	UINT bitmap_stride;
	COM_REQUIRE_SUCCESS(lock->GetStride(&bitmap_stride));

	std::fill_n(bitmap_data, buffer_size, 0xFF);
	seeker.seek(stdext::seek_from::current, 8);

	uint16_t seg_flags;
	while ((seg_flags = stream.read<uint16_t>()) != 0)
	{
		auto seg_width = seg_flags >> 1;

		auto x = stream.read<int16_t>();
		auto y = stream.read<int16_t>();

		x -= dimensions.p1.x;
		y -= dimensions.p1.y;
	
		auto segment_data = bitmap_data + (y * bitmap_stride) + x;
		if ((seg_flags & 1) != 0)
		{
			while (seg_width > 0)
			{
				auto run_flags = stream.read<uint8_t>();
				auto run_width = run_flags >> 1;
				if ((run_flags & 1) != 0)
				{
					auto color = stream.read<uint8_t>();
					std::fill_n(segment_data, run_width, color);
				}
				else
					stream.read(segment_data, run_width);

				seg_width -= run_width;
				segment_data += run_width;
			}
		}
		else
			stream.read(segment_data, seg_width);
	}

	lock.Release();

	IWICStreamPtr wicstream;
	COM_REQUIRE_SUCCESS(imaging_factory->CreateStream(&wicstream));
	COM_REQUIRE_SUCCESS(wicstream->InitializeFromFilename(output_path, GENERIC_WRITE));

	IWICBitmapEncoderPtr encoder;
	COM_REQUIRE_SUCCESS(imaging_factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
	COM_REQUIRE_SUCCESS(encoder->Initialize(wicstream, WICBitmapEncoderNoCache));

	IWICBitmapFrameEncodePtr frame_encode;
	COM_REQUIRE_SUCCESS(encoder->CreateNewFrame(&frame_encode, nullptr));
	COM_REQUIRE_SUCCESS(frame_encode->Initialize(nullptr));
	COM_REQUIRE_SUCCESS(frame_encode->WriteSource(bitmap, nullptr));
	COM_REQUIRE_SUCCESS(frame_encode->Commit());
	COM_REQUIRE_SUCCESS(encoder->Commit());
}

void pack_images(const std::vector<const wchar_t*>& input_paths, game_id game, const std::vector<point>& reference_points, const wchar_t* output_path)
{
    WORD resid = game == game_id::wc1 ? RESOURCE_ID_WC1PAL : RESOURCE_ID_WC2PAL;
    size_t offset = game == game_id::wc1 ? 0x30 : 0;

    auto res = ::FindResource(nullptr, MAKEINTRESOURCE(resid), L"BINARY");
    if (res == nullptr)
        throw std::system_error(::GetLastError(), std::system_category());
    auto resp = ::LoadResource(nullptr, res);
    auto palette_data = ::LockResource(resp);
    WICColor colors[256];

    auto palette_p = static_cast<const BYTE*>(palette_data) + offset;
    auto color_p = colors;
    for (size_t n = 0; n < stdext::lengthof(colors); ++n)
    {
        auto red = *palette_p++;
        auto green = *palette_p++;
        auto blue = *palette_p++;
        *color_p++ = blue | (green << 8) | (red << 16) | (0xFF << 24);
    }
    colors[stdext::lengthof(colors) - 1] &= 0x00FFFFFF; // last entry transparent

    HRESULT hr;
    COM_REQUIRE_SUCCESS(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    at_scope_exit(::CoUninitialize);

    IWICImagingFactoryPtr imaging_factory;
    COM_REQUIRE_SUCCESS(imaging_factory.CreateInstance(CLSID_WICImagingFactory));

    IWICPalettePtr palette;
    COM_REQUIRE_SUCCESS(imaging_factory->CreatePalette(&palette));
    COM_REQUIRE_SUCCESS(palette->InitializeCustom(colors, stdext::lengthof(colors)));

    // Figure out how many images we're writing
    UINT image_count = 0;
    for (const auto& path : input_paths)
    {
        IWICBitmapDecoderPtr decoder;
        COM_REQUIRE_SUCCESS(imaging_factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder));

        UINT frame_count;
        COM_REQUIRE_SUCCESS(decoder->GetFrameCount(&frame_count));
        image_count += frame_count;
    }

    stdext::file_output_stream output(output_path);
    output.write(uint32_t(0));  // file size
    for (size_t n = 0; n < image_count; ++n)
        output.write(uint32_t(0));  // image offset

    stdext::stream_position offset_position = 4;
    auto image_position = output.position();
    output.set_position(offset_position);
    auto reference_point = reference_points.begin();
    for (const auto& path : input_paths)
    {
        IWICBitmapDecoderPtr decoder;
        COM_REQUIRE_SUCCESS(imaging_factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder));

        UINT frame_count;
        COM_REQUIRE_SUCCESS(decoder->GetFrameCount(&frame_count));
        for (UINT n = 0; n < frame_count; ++n)
        {
            IWICBitmapFrameDecodePtr frame;
            COM_REQUIRE_SUCCESS(decoder->GetFrame(n, &frame));

            output.set_position(offset_position);
            output.write(uint32_t(image_position));
            offset_position = output.position();
            output.set_position(image_position);

            pack_image(imaging_factory, palette, frame, *reference_point, output);
            image_position = output.position();
        }

        ++reference_point;
    }

    output.set_position(0);
    output.write(uint32_t(image_position));
}

void pack_image(const IWICImagingFactoryPtr& imaging_factory, const IWICPalettePtr& palette, const IWICBitmapFrameDecodePtr& input, point reference_point, stdext::output_stream& output)
{
    HRESULT hr;
    IWICFormatConverterPtr converter;
    COM_REQUIRE_SUCCESS(imaging_factory->CreateFormatConverter(&converter));
    COM_REQUIRE_SUCCESS(converter->Initialize(input, GUID_WICPixelFormat8bppIndexed, WICBitmapDitherTypeNone, palette, 0.5, WICBitmapPaletteTypeCustom));

    UINT width, height;
    COM_REQUIRE_SUCCESS(converter->GetSize(&width, &height));

    std::vector<BYTE> pixels(width * height);
    COM_REQUIRE_SUCCESS(converter->CopyPixels(nullptr, width, pixels.size(), pixels.data()));

    output.write(int16_t(width - 1 - reference_point.x));   // right
    output.write(int16_t(reference_point.x));               // left
    output.write(int16_t(reference_point.y));               // top
    output.write(int16_t(height - 1 - reference_point.y));  // bottom

    auto p = pixels.data();
    auto p_first = p;
    auto p_last = p + pixels.size();

    // Search for non-transparent pixels
    while ((p = std::find_if(p, p_last, [](BYTE pixel) { return pixel != 0xFF; })) != p_last)
    {
        // Get the location of the segment
        auto offset = p - p_first;
        auto x = offset % width;
        auto y = offset / width;

        // Get the width of the segment
        auto row_last = p_first + width * (y + 1);
        auto seg_first = p;
        auto seg_last = std::find(seg_first, row_last, 0xFF);
        while (p != seg_last)
        {
            // Check for runs in the segment
            auto run_first = p;
            auto run_last = p;
            auto run_length = 0;
            while ((run_first = std::adjacent_find(run_first, seg_last)) != seg_last)
            {
                run_last = std::find_if(run_first, seg_last, [&](BYTE pixel) { return pixel != *run_first; });
                run_length = run_last - run_first;
                if (run_length > 3 || (run_length > 2 && run_last == seg_last))
                    break;

                run_first = run_last;
            }

            if (p == seg_first && run_first == seg_last)
            {
                // No runs; just write the segment verbatim
                auto length = seg_last - seg_first;
                output.write(uint16_t(length << 1));
                output.write(int16_t(x - reference_point.x));
                output.write(int16_t(y - reference_point.y));
                output.write(p, length);
                p = seg_last;
            }
            else
            {
                // Break up the segment into runs and intervening verbatim chunks
                if (p == seg_first)
                {
                    auto length = seg_last - seg_first;
                    output.write(uint16_t((length << 1) | 1));
                    output.write(int16_t(x - reference_point.x));
                    output.write(int16_t(y - reference_point.y));
                }

                if (p != run_first)
                {
                    auto length = run_first - p;
                    output.write(uint8_t(length << 1));
                    output.write(p, length);
                    p = run_first;
                }

                if (run_first < run_last)
                {
                    output.write(uint8_t((run_length << 1) | 1));
                    output.write(*p);
                    p = run_last;
                }
            }
        }
    }

    output.write(uint16_t(0));
}

rect get_image_dimensions(stdext::multi_ref<stdext::input_stream, stdext::seekable> input)
{
    auto& stream = input.as<stdext::input_stream>();
    auto& seeker = input.as<stdext::seekable>();
	auto start_position = seeker.position();

    auto right_extent = stream.read<int16_t>();
    auto left_extent = stream.read<int16_t>();
    auto top_extent = stream.read<int16_t>();
    auto bottom_extent = stream.read<int16_t>();

    rect dim =
	{
		{ -left_extent, -top_extent },
		{ right_extent + 1, bottom_extent + 1 }
	};

	seeker.set_position(start_position);
	return dim;
}

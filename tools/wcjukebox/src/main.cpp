#include "wcaudio_stream.h"
#include "dsound_player.h"

#include <stdext/file.h>
#include <stdext/utility.h>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <cstdlib>
#include <cstddef>


using namespace std::literals;

namespace
{
    enum stream_archive
    {
        preflight, postflight, mission
    };

    enum : uint32_t
    {
        mode_none           = 0x000,
        mode_track          = 0x001,
        mode_trigger        = 0x002,
        mode_intensity      = 0x004,
        mode_stream         = 0x008,
        mode_show_tracks    = 0x010,
        mode_show_triggers  = 0x020,
        mode_wav            = 0x040,
        mode_info           = 0x080,
        mode_loop           = 0x100
    };

    struct program_options
    {
        uint32_t program_mode = mode_none;
        int track = -1;
        const wchar_t* stream_path = nullptr;
        const wchar_t* wav_path = nullptr;
        uint8_t trigger = no_trigger;
        uint8_t intensity = 15; // default for WC1 (selects patrol music)
        int loops = -1;
    };

    class usage_error : public std::runtime_error
    {
        using runtime_error::runtime_error;
    };

    struct game_trigger_desc
    {
        stream_archive archive;
        uint8_t trigger;
    };

    void show_usage(const wchar_t* invocation);
    void diagnose_mode(uint32_t mode);
    void diagnose_unrecognized(const wchar_t* str);
    std::string to_mbstring(const wchar_t* str);
    int parse_int(const wchar_t* str);

    constexpr const wchar_t* stream_filenames[]
    {
        L"STREAMS\\PREFLITE.STR",
        L"STREAMS\\POSFLITE.STR",
        L"STREAMS\\MISSION.STR"
    };

    // Maps from a track number to a stream archive and trigger number
    // See StreamLoadTrack and GetStreamTrack in Wing1.i64
    constexpr game_trigger_desc track_map[] =
    {
        { stream_archive::mission, no_trigger },    // 0 - Combat 1
        { stream_archive::mission, no_trigger },    // 1 - Combat 2
        { stream_archive::mission, no_trigger },    // 2 - Combat 3
        { stream_archive::mission, no_trigger },    // 3 - Combat 4
        { stream_archive::mission, no_trigger },    // 4 - Combat 5
        { stream_archive::mission, no_trigger },    // 5 - Combat 6
        { stream_archive::mission, 6 },             // 6 - Victorious combat
        { stream_archive::mission, 7 },             // 7 - Tragedy
        { stream_archive::mission, 8 },             // 8 - Dire straits
        { stream_archive::mission, 9 },             // 9 - Scratch one fighter
        { stream_archive::mission, 10 },            // 10 - Defeated fleeing enemy
        { stream_archive::mission, 11 },            // 11 - Wingman death
        { stream_archive::mission, no_trigger },    // 12 - Returning defeated
        { stream_archive::mission, no_trigger },    // 13 - Returning successful
        { stream_archive::mission, no_trigger },    // 14 - Returning jubilant
        { stream_archive::mission, no_trigger },    // 15 - Mission 1
        { stream_archive::mission, no_trigger },    // 16 - Mission 2
        { stream_archive::mission, no_trigger },    // 17 - Mission 3
        { stream_archive::mission, no_trigger },    // 18 - Mission 4
        { stream_archive::preflight, no_trigger },  // 19 - OriginFX (actually, fanfare)
        { stream_archive::preflight, 1 },           // 20 - Arcade Mission
        { stream_archive::preflight, 4 },           // 21 - Arcade Victory
        { stream_archive::preflight, 3 },           // 22 - Arcade Death
        { stream_archive::preflight, no_trigger },  // 23 - Fanfare
        { stream_archive::preflight, 5 },           // 24 - Halcyon's Office 1
        { stream_archive::preflight, 6 },           // 25 - Briefing
        { stream_archive::preflight, 7 },           // 26 - Briefing Dismissed
        { stream_archive::mission, 27 },            // 27 - Scramble
        { stream_archive::postflight, no_trigger }, // 28 - Landing
        { stream_archive::postflight, 0, },         // 29 - Damage Assessment
        { stream_archive::preflight, 0 },           // 30 - Rec Room
        { stream_archive::mission, 31 },            // 31 - Eject
        { stream_archive::mission, 32 },            // 32 - Death
        { stream_archive::postflight, 2 },          // 33 - debriefing (successful)
        { stream_archive::postflight, 1 },          // 34 - debriefing (failed)
        { stream_archive::preflight, 2 },           // 35 - barracks
        { stream_archive::postflight, 3 },          // 36 - Halcyon's Office / Briefing 2
        { stream_archive::postflight, 4 },          // 37 - medal (valor?)
        { stream_archive::postflight, 5 },          // 38 - medal (golden sun?)
        { stream_archive::postflight, 7 },          // 39 - another medal
        { stream_archive::postflight, 6 },          // 40 - big medal
    };
}

int wmain(int argc, wchar_t* argv[])
{
    std::wstring invocation = argc > 0 ? std::filesystem::path(argv[0]).filename() : "wcjukebox";

    try
    {
        if (argc < 2)
        {
            show_usage(invocation.c_str());
            return EXIT_SUCCESS;
        }

        program_options options;
        for (const wchar_t* const* arg = &argv[1]; *arg != nullptr; ++arg)
        {
            if ((*arg)[0] == L'-' || (*arg)[0] == L'/')
            {
                if (*arg + 1 == L"track"sv)
                {
                    if ((options.program_mode & mode_track) != 0)
                        throw usage_error("The -track option can only be used once.");

                    options.program_mode |= mode_track;
                    diagnose_mode(options.program_mode);
                    options.track = parse_int(*++arg);
                    if (options.track < 0 || unsigned(options.track) >= stdext::lengthof(track_map))
                    {
                        std::ostringstream message;
                        message << "Track must be between 0 and " << stdext::lengthof(track_map) - 1 << '.';
                        throw usage_error(std::move(message).str());
                    }
                }
                else if (*arg + 1 == L"trigger"sv)
                {
                    if ((options.program_mode & mode_trigger) != 0)
                        throw usage_error("The -trigger option can only be used once.");

                    options.program_mode |= mode_trigger;
                    diagnose_mode(options.program_mode);
                    auto value = parse_int(*++arg);
                    if (value < 0 || unsigned(value) > 0xFF)
                        throw usage_error("Trigger must be between 0 and 255.");
                    options.trigger = uint8_t(value);
                }
                else if (*arg + 1 == L"intensity"sv)
                {
                    if ((options.program_mode & mode_intensity) != 0)
                        throw usage_error("The -intensity option can only be used once.");

                    options.program_mode |= mode_intensity;
                    diagnose_mode(options.program_mode);
                    auto value = parse_int(*++arg);
                    if (value < 0 || unsigned(value) > 100)
                        throw usage_error("Intensity must be between 0 and 100.");
                    options.intensity = uint8_t(value);
                }
                else if (*arg + 1 == L"o"sv)
                {
                    if ((options.program_mode & mode_wav) != 0)
                        throw usage_error("The -o option can only be used once.");

                    options.program_mode |= mode_wav;
                    diagnose_mode(options.program_mode);
                    options.wav_path = *++arg;
                    if (options.wav_path == nullptr)
                        throw usage_error("Expected WAV file path.");
                }
                else if (*arg + 1 == L"info"sv)
                {
                    if ((options.program_mode & mode_info) != 0)
                        throw usage_error("The -info option can only be used once.");

                    options.program_mode |= mode_info;
                    diagnose_mode(options.program_mode);
                }
                else if (*arg + 1 == L"loop"sv)
                {
                    if ((options.program_mode & mode_loop) != 0)
                        throw usage_error("The -loop option can only be used once.");

                    options.program_mode |= mode_loop;
                    diagnose_mode(options.program_mode);
                    options.loops = parse_int(*++arg);
                    if (options.loops < 0)
                        throw usage_error("The -loop option cannot be negative.");
                }
                else
                    diagnose_unrecognized(*arg);
            }
            else
            {
                if ((options.program_mode & mode_stream) != 0)
                    diagnose_unrecognized(*arg);

                options.program_mode |= mode_stream;
                diagnose_mode(options.program_mode);
                options.stream_path = *arg;
            }
        }

        if ((options.program_mode & (mode_track | mode_stream | mode_show_tracks | mode_show_triggers)) == 0)
            throw usage_error("Missing required options.");

        if ((options.program_mode & mode_track) != 0)
        {
            options.stream_path = stream_filenames[track_map[options.track].archive];
            options.trigger = track_map[options.track].trigger;
            if (track_map[options.track].archive == stream_archive::mission && options.trigger == no_trigger)
                options.intensity = uint8_t(options.track);
        }

        stdext::file_input_stream file(options.stream_path);
        wcaudio_stream stream(file);
        stream.select(options.trigger, options.intensity);

        auto& header = stream.file_header();
        dsound_player player;
        player.reset(header.channels, header.sample_rate, header.bits_per_sample, header.buffer_size);
        player.play(stream);

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
    catch (const _com_error& e)
    {
        std::wcerr << L"Error: " << e.ErrorMessage() << '\n';
    }
    catch (...)
    {
        std::cerr << "Unknown error\n";
    }

    return EXIT_FAILURE;
}

namespace
{
    void show_usage(const wchar_t* invocation)
    {
        std::wcout << L"Usage:\n"
            L"  " << invocation << L" [<options>...] -track <num>\n"
            L"  " << invocation << L" [<options>...] [-trigger <num>] [-intensity <num>] <filename>\n"
            L"  " << invocation << L" -show-tracks\n"
            L"  " << invocation << L" -show-triggers <filename>\n"
            L"\n"
            L"The first form selects a music track to play.  The command must be invoked from\n"
            L"the game directory (the same directory containing the STREAMS directory).  The\n"
            L"correct stream file will be loaded automatically based on an internal mapping\n"
            L"from track number to stream file, trigger, and intensity values.  To view the\n"
            L"mapping, use the -show-tracks option.  Currently, tracks are only supported for\n"
            L"WC1.\n"
            L"\n"
            L"The second form selects a track using the provided trigger and intensity values\n"
            L"for the given stream file.  If the trigger is not provided, " << invocation << L"will play\n"
            L"from the first piece of audio data contained in the stream.  If the intensity\n"
            L"value is not provided, a default value will be used.  To view the list of\n"
            L"triggers and intensities supported by a given stream file, use the\n"
            L"-show-triggers option.  This form may be used with any stream file.\n"
            L"\n"
            L"Options:\n"
            L"  -o <filename>\n"
            L"    Instead of playing music, write it to a WAV file.\n"
            L"\n"
            L"  -info\n"
            L"    Display information related to playback.  The stream contains embedded\n"
            L"    information that tells the player how to loop a track or how to progress\n"
            L"    from one track to another.  This information will be printed out as it is\n"
            L"    encountered.  If the -o option is being used, this option will also print\n"
            L"    corresponding frame numbers in the output file.\n"
            L"\n"
            L"  -loop <num>\n"
            L"    Continue playback until <num> loops have been completed.  For instance,\n"
            L"    -loop 0 will disable looping (causing a track to be played only once), and\n"
            L"    -loop 1 will cause the track to repeat once (provided it has a loop point).\n"
            L"    If the track does not have a loop point, this option is ignored.\n";
    }

    void diagnose_mode(uint32_t mode)
    {
        if ((mode & (mode_track | mode_trigger)) == (mode_track | mode_trigger))
            throw usage_error("The -trigger option cannot be used with -track.");
        if ((mode & (mode_track | mode_intensity)) == (mode_track | mode_intensity))
            throw usage_error("The -intensity option cannot be used with -track.");
        if ((mode & (mode_track | mode_stream)) == (mode_track | mode_stream))
            throw usage_error("Cannot specify a stream file with -track.");
        if ((mode & mode_show_tracks) != 0 && mode != mode_show_tracks)
            throw usage_error("The -show-tracks option cannot be used with other options.");
        if ((mode & mode_show_triggers) != 0 && (mode & ~mode_stream) != mode_show_triggers)
            throw usage_error("The -show-triggers option cannot be used with other options.");
    }

    void diagnose_unrecognized(const wchar_t* str)
    {
        std::ostringstream message;
        message << "Unexpected option: " << str;
        throw usage_error(std::move(message).str());
    }

    std::string to_mbstring(const wchar_t* str)
    {
        std::mbstate_t state = {};
        auto length = std::wcsrtombs(nullptr, &str, 0, &state);
        if (length == size_t(-1))
            throw std::runtime_error("Unicode error");

        std::string result(length, '\0');
        std::wcsrtombs(result.data(), &str, length, &state);
        return result;
    }

    int parse_int(const wchar_t* str)
    {
        if (str == nullptr)
            throw usage_error("Expected number.");

        wchar_t* end;
        auto value = int(wcstol(str, &end, 10));
        if (*end != '\0')
        {
            std::ostringstream message;
            message << "Unexpected argument: " << to_mbstring(str) << " (Expected number.)\n";
            throw usage_error(std::move(message).str());
        }

        return value;
    }
}

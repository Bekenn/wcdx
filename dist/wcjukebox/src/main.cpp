#include "wcaudio_stream.h"
#include "wave.h"

#include <stdext/array_view.h>
#include <stdext/file.h>
#include <stdext/scope_guard.h>
#include <stdext/string.h>
#include <stdext/utility.h>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <cstdlib>
#include <cstddef>


using namespace std::literals;

namespace
{
    enum : uint32_t
    {
        mode_none           = 0x000,
        mode_track          = 0x001,
        mode_wc2            = 0x002,
        mode_trigger        = 0x004,
        mode_intensity      = 0x008,
        mode_stream         = 0x010,
        mode_show_tracks    = 0x020,
        mode_show_triggers  = 0x040,
        mode_wav            = 0x080,
        mode_loop           = 0x100,
        mode_single         = 0x200,
        mode_debug_info     = 0x400
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

    void show_usage(const wchar_t* invocation);
    void diagnose_mode(uint32_t mode);
    void diagnose_unrecognized(const wchar_t* str);
    void show_tracks(program_options& options);
    void select_track(program_options& options);
    int parse_int(const wchar_t* str);
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

                    auto game = *++arg;
                    if (game == L"wc2"sv)
                        options.program_mode |= mode_wc2;
                    else if (game != L"wc1"sv)
                        throw usage_error("The -track option must be followed by 'wc1' or 'wc2'.");

                    options.track = parse_int(*++arg);
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
                else if (*arg + 1 == L"show-tracks"sv)
                {
                    if ((options.program_mode & mode_show_tracks) != 0)
                        throw usage_error("The -show-tracks option can only be used once.");

                    options.program_mode |= mode_show_tracks;
                    diagnose_mode(options.program_mode);

                    auto game = *++arg;
                    if (game == L"wc2"sv)
                        options.program_mode |= mode_wc2;
                    else if (game != L"wc1"sv)
                        throw usage_error("The -show-tracks option must be followed by 'wc1' or 'wc2'.");
                }
                else if (*arg + 1 == L"show-triggers"sv)
                {
                    if ((options.program_mode & mode_show_triggers) != 0)
                        throw usage_error("The -show-triggers option can only be used once.");

                    options.program_mode |= mode_show_triggers;
                    diagnose_mode(options.program_mode);
                    options.stream_path = *++arg;
                    if (options.stream_path == nullptr)
                        throw usage_error("Expected STR file path.");
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
                else if (*arg + 1 == L"single"sv)
                {
                    if ((options.program_mode & mode_single) != 0)
                        throw usage_error("The -single option can only be used once.");

                    options.program_mode |= mode_single;
                    diagnose_mode(options.program_mode);
                }
                else if (*arg + 1 == L"debug-info"sv)
                {
                    if ((options.program_mode & mode_debug_info) != 0)
                        throw usage_error("The debug-info option can only be used once.");

                    options.program_mode |= mode_debug_info;
                    diagnose_mode(options.program_mode);
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

        if ((options.program_mode & mode_show_tracks) != 0)
        {
            show_tracks(options);
            return EXIT_SUCCESS;
        }

        if ((options.program_mode & mode_track) != 0)
            select_track(options);

        stdext::file_input_stream file(options.stream_path);
        wcaudio_stream stream(file);

        if ((options.program_mode & mode_show_triggers) != 0)
        {
            std::cout << "Available triggers:";
            for (auto trigger : stream.triggers())
                std::cout << ' ' << unsigned(trigger);
            std::cout << "\nAvailable intensities:";
            for (auto intensity : stream.intensities())
                std::cout << ' ' << unsigned(intensity);
            std::cout << '\n';
            return EXIT_SUCCESS;
        }

        std::unordered_map<uint32_t, unsigned> chunk_frame_map;
        chunk_frame_map.reserve(128);

        stream.on_next_chunk([&](uint32_t chunk_index, unsigned frame_count)
        {
            chunk_frame_map.insert({ chunk_index, frame_count });
        });
        stream.on_loop([&](uint32_t chunk_index, unsigned frame_count)
        {
            stdext::discard(frame_count);
            if ((options.program_mode & mode_debug_info) != 0)
            {
                std::cout << "Loop to chunk " << chunk_index
                    << " (frame index " << chunk_frame_map[chunk_index] << ')'
                    << std::endl;
            }
            return options.loops < 0 || options.loops-- != 0;
        });
        stream.on_start_track([&](uint32_t chunk_index)
        {
            if ((options.program_mode & mode_debug_info) != 0)
                std::cout << "Start track at chunk " << chunk_index << std::endl;
            chunk_frame_map.insert({ chunk_index, 0 });
        });
        stream.on_next_track([&](uint32_t chunk_index, unsigned frame_count)
        {
            chunk_frame_map.insert({ chunk_index, frame_count });
            if ((options.program_mode & mode_debug_info) != 0)
                std::cout << "Switch to track at chunk " << chunk_index << std::endl;
            return (options.program_mode & mode_single) == 0;
        });
        stream.on_prev_track([&](unsigned frame_count)
        {
            stdext::discard(frame_count);
            if ((options.program_mode & mode_debug_info) != 0)
                std::cout << "Return to previous track" << std::endl;
        });
        stream.on_end_of_stream([&](unsigned frame_count)
        {
            stdext::discard(frame_count);
            if ((options.program_mode & mode_debug_info) != 0)
                std::cout << "End of stream" << std::endl;
        });

        if ((options.program_mode & mode_wav) == 0)
            std::cout << "Press Ctrl-C to end playback." << std::endl;

        stream.select(options.trigger, options.intensity);

        if ((options.program_mode & mode_wav) != 0)
        {
            if (options.loops < 0)
                options.loops = 0;
            stdext::file_output_stream out(options.wav_path);
            write_wave(out, stream, stream.channels(), stream.sample_rate(), stream.bits_per_sample(), stream.buffer_size());
        }
        else
            play_wave(stream, stream.channels(), stream.sample_rate(), stream.bits_per_sample(), stream.buffer_size());

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
        std::cerr << "Unknown error\n";
    }

    return EXIT_FAILURE;
}

namespace
{
    enum stream_archive
    {
        invalid = -1,
        wc1_preflight, wc1_postflight, wc1_mission,
        wc2_gameflow, wc2_gametwo, wc2_spaceflight
    };

    struct track_desc
    {
        stream_archive archive;
        uint8_t trigger;
    };

    constexpr const wchar_t* stream_filenames[]
    {
        L"STREAMS\\PREFLITE.STR",
        L"STREAMS\\POSFLITE.STR",
        L"STREAMS\\MISSION.STR",
        L"STREAMS\\GAMEFLOW.STR",
        L"STREAMS\\GAMETWO.STR",
        L"STREAMS\\SPACEFLT.STR"
    };

    // Maps from a track number to a stream archive and trigger number
    // See StreamLoadTrack and GetStreamTrack in Wing1.i64
    constexpr track_desc wc1_track_map[] =
    {
        { stream_archive::wc1_mission, no_trigger },    // 0 - Regular Combat
        { stream_archive::wc1_mission, no_trigger },    // 1 - Being Tailed
        { stream_archive::wc1_mission, no_trigger },    // 2 - Tailing An Enemy
        { stream_archive::wc1_mission, no_trigger },    // 3 - Missile Tracking You
        { stream_archive::wc1_mission, no_trigger },    // 4 - You're Severely Damaged - Floundering
        { stream_archive::wc1_mission, no_trigger },    // 5 - Intense Combat
        { stream_archive::wc1_mission, 6 },             // 6 - Target Hit
        { stream_archive::wc1_mission, 7 },             // 7 - Ally Killed
        { stream_archive::wc1_mission, 8 },             // 8 - Your Wingman's been hit
        { stream_archive::wc1_mission, 9 },             // 9 - Enemy Ace Killed
        { stream_archive::wc1_mission, 10 },            // 10 - Overall Victory
        { stream_archive::wc1_mission, 11 },            // 11 - Overall Defeat
        { stream_archive::wc1_mission, no_trigger },    // 12 - Returning Defeated
        { stream_archive::wc1_mission, no_trigger },    // 13 - Returning Normal
        { stream_archive::wc1_mission, no_trigger },    // 14 - Returning Triumphant
        { stream_archive::wc1_mission, no_trigger },    // 15 - Flying to Dogfight
        { stream_archive::wc1_mission, no_trigger },    // 16 - Goal Line - Defending the Claw
        { stream_archive::wc1_mission, no_trigger },    // 17 - Strike Mission - Go Get 'Em
        { stream_archive::wc1_mission, no_trigger },    // 18 - Grim or Escort Mission
        { stream_archive::wc1_preflight, no_trigger },  // 19 - OriginFX (actually, fanfare)
        { stream_archive::wc1_preflight, 1 },           // 20 - Arcade Theme
        { stream_archive::wc1_preflight, 4 },           // 21 - Arcade Victory
        { stream_archive::wc1_preflight, 3 },           // 22 - Arcade Death
        { stream_archive::wc1_preflight, no_trigger },  // 23 - Fanfare
        { stream_archive::wc1_preflight, 5 },           // 24 - Briefing intro
        { stream_archive::wc1_preflight, 6 },           // 25 - Briefing middle
        { stream_archive::wc1_preflight, 7 },           // 26 - Briefing end
        { stream_archive::wc1_mission, 27 },            // 27 - Scramble through launch
        { stream_archive::wc1_postflight, no_trigger }, // 28 - Landing
        { stream_archive::wc1_postflight, 0, },         // 29 - Medium Damage Assessment
        { stream_archive::wc1_preflight, 0 },           // 30 - Rec Room
        { stream_archive::wc1_mission, 31 },            // 31 - Eject - Imminent Rescue
        { stream_archive::wc1_mission, 32 },            // 32 - Funeral
        { stream_archive::wc1_postflight, 2 },          // 33 - Debriefing - Successful
        { stream_archive::wc1_postflight, 1 },          // 34 - Debriefing - Unsuccessful
        { stream_archive::wc1_preflight, 2 },           // 35 - Barracks - Go To Sleep You Pilots
        { stream_archive::wc1_postflight, 3 },          // 36 - Commander's Office
        { stream_archive::wc1_postflight, 4 },          // 37 - Medel Ceremony - General
        { stream_archive::wc1_postflight, 5 },          // 38 - Medal Ceremony - Purple Heart
        { stream_archive::wc1_postflight, 7 },          // 39 - Minor Bravery
        { stream_archive::wc1_postflight, 6 },          // 40 - Major Bravery
    };

    constexpr track_desc wc2_track_map[] =
    {
        { stream_archive::wc2_spaceflight, no_trigger },    // 0 - Combat 1
        { stream_archive::wc2_spaceflight, no_trigger },    // 1 - Combat 2
        { stream_archive::wc2_spaceflight, no_trigger },    // 2 - Combat 3
        { stream_archive::wc2_spaceflight, no_trigger },    // 3 - Combat 4
        { stream_archive::wc2_spaceflight, no_trigger },    // 4 - Combat 5
        { stream_archive::wc2_spaceflight, no_trigger },    // 5 - Combat 6
        { stream_archive::wc2_spaceflight, 6 },             // 6 - Victorious Combat
        { stream_archive::wc2_spaceflight, 7 },             // 7 - Tragedy
        { stream_archive::wc2_spaceflight, 8 },             // 8 - Dire straits
        { stream_archive::wc2_spaceflight, 9 },             // 9 - Scratch one fighter
        { stream_archive::wc2_spaceflight, 10 },            // 10 - Defeated fleeing enemy
        { stream_archive::wc2_spaceflight, 11 },            // 11 - Wingman death
        { stream_archive::wc2_spaceflight, no_trigger },    // 12 - Returning defeated
        { stream_archive::wc2_spaceflight, no_trigger },    // 13 - Returning successful
        { stream_archive::wc2_spaceflight, no_trigger },    // 14 - Returning jubilant
        { stream_archive::wc2_spaceflight, no_trigger },    // 15 - Mission 1
        { stream_archive::wc2_spaceflight, no_trigger },    // 16 - Mission 2
        { stream_archive::wc2_spaceflight, no_trigger },    // 17 - Mission 3
        { stream_archive::wc2_spaceflight, no_trigger },    // 18 - Mission 4
        { stream_archive::invalid, no_trigger },
        { stream_archive::invalid, no_trigger },
        { stream_archive::invalid, no_trigger },
        { stream_archive::invalid, no_trigger },
        { stream_archive::invalid, no_trigger },
        { stream_archive::invalid, no_trigger },
        { stream_archive::invalid, no_trigger },
        { stream_archive::invalid, no_trigger },
        { stream_archive::wc2_spaceflight, no_trigger },    // 27 - Scramble
        { stream_archive::wc2_gametwo, 28 },                // 28 - Landing
        { stream_archive::wc2_gametwo, 29 },                // 29 - Damage Assessment
        { stream_archive::invalid, no_trigger },
        { stream_archive::wc2_spaceflight, no_trigger },    // 31 - Eject
        { stream_archive::wc2_spaceflight, no_trigger },    // 32 - Death
        { stream_archive::wc2_gametwo, 33 },                // 33 - debriefing (successful)
        { stream_archive::wc2_gametwo, 34 },                // 34 - debriefing (failed)
        { stream_archive::invalid, no_trigger },
        { stream_archive::wc2_gametwo, 36 },                // 36 - Briefing 2
        { stream_archive::wc2_gametwo, 37 },                // 37 - medal (valor?)
        { stream_archive::wc2_gametwo, 38 },                // 38 - medal (golden sun?)
        { stream_archive::wc2_gametwo, 39 },                // 39 - another medal
        { stream_archive::wc2_gametwo, 40 },                // 40 - big medal
        { stream_archive::wc2_spaceflight, no_trigger },    // 41 - Prologue
        { stream_archive::wc2_spaceflight, 42 },            // 42 - Torpedo lock
        { stream_archive::wc2_gameflow, 43 },               // 43 - Flight deck 1
        { stream_archive::wc2_gameflow, 44 },               // 44 - Angel
        { stream_archive::wc2_gameflow, 45 },               // 45 - Jazz 1
        { stream_archive::wc2_gameflow, 46 },               // 46 - Briefing
        { stream_archive::wc2_gameflow, 47 },               // 47 - Jump
        { stream_archive::wc2_gameflow, 48 },               // 48 - Prologue (quieter)
        { stream_archive::wc2_gameflow, 49 },               // 49 - Lounge 1
        { stream_archive::wc2_gameflow, 50 },               // 50 - Jazz 2
        { stream_archive::wc2_gameflow, 51 },               // 51 - Jazz 3
        { stream_archive::wc2_gameflow, 52 },               // 52 - Jazz 4
        { stream_archive::wc2_gameflow, 53 },               // 53 - Interlude 1
        { stream_archive::wc2_gameflow, 54 },               // 54 - Theme
        { stream_archive::wc2_spaceflight, no_trigger },    // 55 - Bombing run
        { stream_archive::wc2_spaceflight, no_trigger },    // 56 - Final Mission
        { stream_archive::wc2_spaceflight, no_trigger },    // 57 - Fighting Thrakhath
        { stream_archive::wc2_gameflow, 58 },               // 58 - Kilrathi Theme
        { stream_archive::wc2_gametwo, 59 },                // 59 - Good Ending
        { stream_archive::wc2_gametwo, 60 },                // 60 - Lounge 2
        { stream_archive::wc2_gameflow, 61 },               // 61 - End Credits
        { stream_archive::wc2_gameflow, 62 },               // 62 - Interlude 2
        { stream_archive::wc2_gametwo, 63 },                // 63 - Jazz 5
        { stream_archive::wc2_gametwo, 20 },                // 64 - Flight Deck 2
        { stream_archive::wc2_gametwo, 21 },                // 65 - Sabotage

        // Bonus tracks
        { stream_archive::wc2_gameflow, 59 },               // 66 - Defeated fleeing enemy (alternate)
        { stream_archive::wc2_gameflow, 60 },               // 67 - Wingman death (alternate)
        { stream_archive::wc2_gameflow, 63 },               // 68 - Unknown
        { stream_archive::wc2_spaceflight, no_trigger },    // 69 - Jump (looping)
    };

    void show_usage(const wchar_t* invocation)
    {
        std::wcout << L"Usage:\n"
            L"  " << invocation << L" [<options>...] -track (wc1|wc2) <num>\n"
            L"  " << invocation << L" [<options>...] -trigger <num> <filename>\n"
            L"  " << invocation << L" -show-tracks (wc1|wc2)\n"
            L"  " << invocation << L" -show-triggers <filename>\n"
            L"\n"
            L"The first form selects a music track to play.  The command must be invoked from\n"
            L"the game directory (the same directory containing the STREAMS directory).  The\n"
            L"correct stream file will be loaded automatically based on an internal mapping\n"
            L"from track number to stream file, trigger, and intensity values.  To view the\n"
            L"mapping, use the -show-tracks option.\n"
            L"\n"
            L"The second form selects a track using the provided trigger value for the given\n"
            L"stream file.  If the trigger is not provided, " << invocation << L" will play from the\n"
            L"first piece of audio data contained in the stream.  If the intensity value is\n"
            L"not provided, a default value will be used.  To view the list of triggers and\n"
            L"intensities supported by a given stream file, use the -show-triggers option.\n"
            L"This form may be used with any stream file.\n"
            L"\n"
            L"Options:\n"
            L"  -o <filename>\n"
            L"    Instead of playing music, write it to a WAV file.\n"
            L"\n"
            L"  -intensity <num>\n"
            L"    This value is used by the playback engine to handle transitions between\n"
            L"    tracks.  Some tracks are designed to transition to other specific tracks\n"
            L"    upon completion, and this value determines which one that is.  For example,\n"
            L"    the scramble music from WC1 will transition to a track appropriate to a\n"
            L"    given mission type based on the intensity value.  If this value is not\n"
            L"    provided, a default value will be used.  For a list of supported triggers\n"
            L"    and intensity values, use the -show-triggers option.\n"
            L"\n"
            L"  -loop <num>\n"
            L"    Continue playback until <num> loops have been completed.  For instance,\n"
            L"    -loop 0 will disable looping (causing a track to be played only once), and\n"
            L"    -loop 1 will cause the track to repeat once (provided it has a loop point).\n"
            L"    If the track does not have a loop point, this option is ignored.  If this\n"
            L"    option is not specified, the track will loop indefinitely.\n"
            L"\n"
            L"  -single\n"
            L"    Stop playback at transition points instead of following the transition to\n"
            L"    the next track.\n"
            L"\n"
            L"  -debug-info\n"
            L"    Display information related to playback.  The stream contains embedded\n"
            L"    information that tells the player how to loop a track or how to progress\n"
            L"    from one track to another.  This information will be printed out as it is\n"
            L"    encountered.  If the -o option is being used, this option will also print\n"
            L"    corresponding frame numbers in the output file.\n";
    }

    void diagnose_mode(uint32_t mode)
    {
        if ((mode & (mode_track | mode_trigger)) == (mode_track | mode_trigger))
            throw usage_error("The -trigger option cannot be used with -track.");
        if ((mode & (mode_track | mode_stream)) == (mode_track | mode_stream))
            throw usage_error("Cannot specify a stream file with -track.");
        if ((mode & mode_show_tracks) != 0 && mode != mode_show_tracks)
            throw usage_error("The -show-tracks option cannot be used with other options.");
        if ((mode & mode_show_triggers) != 0 && (mode & ~mode_stream) != mode_show_triggers)
            throw usage_error("The -show-triggers option cannot be used with other options.");
    }

    void diagnose_unrecognized(const wchar_t* str)
    {
        std::wostringstream message;
        message << L"Unexpected option: " << str;
        throw usage_error(stdext::to_mbstring(std::move(message).str().c_str()));
    }

    void show_tracks(program_options& options)
    {
        stdext::array_view<const track_desc> track_map;
        if ((options.program_mode & mode_wc2) == 0)
            track_map = wc1_track_map;
        else
            track_map = wc2_track_map;

        std::wcout <<
            L"Track |         File         | Trigger | Intensity\n"
            L"------|----------------------|---------|----------\n";
        unsigned track_number = 0;
        for (auto entry : track_map)
        {
            at_scope_exit([&]{ ++track_number; });
            if (entry.archive == stream_archive::invalid)
                continue;

            std::wcout << std::setw(5) << track_number << L" | ";
            std::wcout << std::setw(20) << std::setiosflags(std::ios_base::left) << stream_filenames[entry.archive] << L" | ";

            std::wcout.unsetf(std::ios_base::adjustfield);
            std::wcout.width(7);
            if (entry.trigger == no_trigger)
                std::wcout << L"";
            else
                std::wcout << entry.trigger;
            std::wcout << L" | ";

            std::wcout.width(9);
            if (entry.trigger == no_trigger)
                std::wcout << (track_number == 69 ? 47 : track_number);
            else
                std::wcout << L"";

            std::wcout << L'\n';
        }
    }

    void select_track(program_options& options)
    {
        stdext::array_view<const track_desc> track_map;
        if ((options.program_mode & mode_wc2) == 0)
            track_map = wc1_track_map;
        else
            track_map = wc2_track_map;

        if (options.track < 0 || unsigned(options.track) >= track_map.size())
        {
            std::ostringstream message;
            message << "Track must be between 0 and " << track_map.size() - 1 << '.';
            throw usage_error(std::move(message).str());
        }

        if (track_map[options.track].archive == stream_archive::invalid)
        {
            std::ostringstream message;
            message << "There is no track " << options.track << '.';
            throw std::runtime_error(std::move(message).str());
        }

        options.stream_path = stream_filenames[track_map[options.track].archive];
        options.trigger = track_map[options.track].trigger;
        if (options.trigger == no_trigger)
        {
            // There's no easy way to map this one, so hard-code it instead.
            if (options.track == 69)
                options.intensity = 47;
            else
                options.intensity = uint8_t(options.track);
        }
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
            message << "Unexpected argument: " << stdext::to_mbstring(str) << " (Expected number.)\n";
            throw usage_error(std::move(message).str());
        }

        return value;
    }
}

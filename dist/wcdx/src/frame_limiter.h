//*****************************************frame_limiter.h*****************************************
// Frame Limiter that can be used to limit the framerate of the main thread.
//
// Usage:
//
// Frame_Limiter()               - Call at the end of each frame to enforce the framerate limit.
// Frame_Limiter_Set_FPS(fps)    - Change the target FPS.
//
//*************************************************************************************************

#pragma once
#include <atomic>
#include <cmath>
#include <functional>

#include <Windows.h>
#include <unordered_set>
#include <mutex>
#include <mmsystem.h>

// Need winmm.lib for timeBeginPeriod and timeEndPeriod
#pragma comment(lib, "winmm.lib")

// Default value for frame limiter target fps. 32FPS is the maximum space framerate Alt+N, Alt+M
inline int FRAME_MAX = 32;

//============================================ Frame Limiter ============================================

struct FrameLimiter
{
        std::atomic<int> targetFps{ FRAME_MAX };
        LARGE_INTEGER frequency{};
        LONGLONG previous{ 0 };

        FrameLimiter()
        {
            timeBeginPeriod(1);
            QueryPerformanceFrequency(&frequency);
            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&previous));
        }

        ~FrameLimiter()
        {
            timeEndPeriod(1);
        }

        void onFrame(std::function<void(DWORD)> sleeper)
        {
            if (targetFps.load() <= 0)
            {
                QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&previous));
                return;
            }

            LARGE_INTEGER current;
            QueryPerformanceCounter(&current);
            double elapsedMs = double(current.QuadPart - previous) * 1000.0 / double(frequency.QuadPart);
            double targetMs = 1000.0 / double(targetFps.load());
            if (elapsedMs < targetMs)
            {
                double remaining = targetMs - elapsedMs;

                if (remaining > 2.0)
                {
                    DWORD sleepMs = static_cast<DWORD>(std::floor(remaining)) - 1;
                    if (sleepMs > 0)
                        sleeper(sleepMs);
                }

                do
                {
                    QueryPerformanceCounter(&current);
                    elapsedMs = double(current.QuadPart - previous) * 1000.0 / double(frequency.QuadPart);
                } while (elapsedMs < targetMs);
            }

            previous = current.QuadPart;
        }
    };

inline FrameLimiter g_frameLimiter;

inline void Frame_Limiter_Set_FPS(int fps) { 
    g_frameLimiter.targetFps.store(fps); 
}

inline void Frame_Limiter(){
    g_frameLimiter.onFrame([](DWORD ms){ ::Sleep(ms); });
}
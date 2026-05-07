#pragma once

#include "wcdx.h"
    
inline bool wcdx_ini_Save(const char* path) {
    FILE* file = nullptr;
    if (fopen_s(&file, path, "w") != 0 || !file) return false;
    fprintf(file,
        "; =============================================================================================\n"
        "; wcdx settings\n"
        "; =============================================================================================\n"
        "; FullScreen: Start in fullscreen mode (1) or windowed mode (0)\n"
        "FullScreen=%d\n"
        "; AspectRatioX: Display aspect ratio X (4). WARNING! Changing this will cause image distortion.\n"
        "AspectRatioX=%ld\n"
        "; AspectRatioY: Display aspect ratio Y (3). WARNING! Changing this will cause image distortion.\n"
        "AspectRatioY=%ld",
        (int)exposed_fullScreen, exposed_AspectRatioX, exposed_AspectRatioY);
    return fclose(file) == 0;
}

inline bool wcdx_ini_Load(const char* path) {
    FILE* file = nullptr;
    if (fopen_s(&file, path, "r") != 0 || !file)
        return wcdx_ini_Save(path);

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == ';' || line[0] == '#' || line[0] == '\n')
            continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        if (strcmp(line, "FullScreen") == 0)
            exposed_fullScreen = atoi(eq + 1) != 0;
        else if (strcmp(line, "AspectRatioX") == 0)
            exposed_AspectRatioX = atol(eq + 1);
        else if (strcmp(line, "AspectRatioY") == 0)
            exposed_AspectRatioY = atol(eq + 1);
    }
    fclose(file);
    return true;
}
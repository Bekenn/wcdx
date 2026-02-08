//********************************************wcdx_ini.h*******************************************
// Simple INI loader/saver for custom user settings.
//*************************************************************************************************
#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

inline std::string trim(const std::string &s) {
    const char *ws = " \t\n\r";
    auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) return std::string();
    auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

// Save settings to `path`. Returns true on success.
inline bool wcdx_ini_Save(const std::string &path = "wcdx.ini") {
    std::ofstream ofs(path, std::ofstream::trunc);
    if (!ofs) return false;
    ofs << "; =============================================================================================\n";
    ofs << "; wcdx settings\n";
    ofs << "; =============================================================================================\n";
    return ofs.good();
}

// Load settings from `path`. If file does not exist a default file is created and saved.
inline bool wcdx_ini_Load(const std::string &path = "wcdx.ini") {
    namespace fs = std::filesystem;
    if (!fs::exists(path)) {
        return wcdx_ini_Save(path);
    }

    std::ifstream ifs(path);
    if (!ifs) return false;

    std::string line;
    while (std::getline(ifs, line)) {
        // strip comments
        auto comment_pos = line.find_first_of(";#");
        if (comment_pos != std::string::npos) line.erase(comment_pos);
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = trim(line.substr(0, eq));
        auto val = trim(line.substr(eq + 1));
        if (key.empty() || val.empty()) continue;
        try {
            // Load settings here
        } catch (...) {
        }
    }

    return true;
}
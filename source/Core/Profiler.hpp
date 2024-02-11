#pragma once

#include "optick.h"
#include <chrono>

struct TimeScope {
    std::string title;

    TimeScope(const std::string& title, bool logged = false) {
        this->title = title;
        start = std::chrono::high_resolution_clock::now();
        this->logged = logged;
    }

    ~TimeScope();

    static const std::unordered_map<std::string, float>& GetCPUTimes();

private:
    std::chrono::high_resolution_clock::time_point start;
    bool logged = false;
};

#define LUZ_PROFILE_FRAME()      OPTICK_FRAME("MainThread")
#define LUZ_PROFILE_FUNC()       OPTICK_EVENT()
#define LUZ_PROFILE_NAMED(name) TimeScope t((name));
#define LUZ_PROFILE_THREAD(name) OPTICK_EVENT((name))
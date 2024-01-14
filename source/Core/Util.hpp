#pragma once

#include "Base.hpp"

std::string EncodeBase64(unsigned char const* input, size_t len);
std::vector<u8> DecodeBase64(std::string const& input);

struct TimeScope {
    std::string title;
    std::chrono::steady_clock::time_point start;

    TimeScope(const std::string& title)
        : title(title)
        , start(std::chrono::high_resolution_clock::now())
    {}

    ~TimeScope() {
        auto now = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() / 1000.0f;
        LOG_INFO("{} took {} seconds", title.c_str(), elapsed);
    }
};
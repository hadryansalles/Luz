#pragma once

#include "Base.hpp"

std::string EncodeBase64(unsigned char const* input, size_t len);
std::vector<u8> DecodeBase64(std::string const& input);

struct TimeScope {
    std::string title;
    std::chrono::steady_clock::time_point start;

    TimeScope(const std::string& title);
    ~TimeScope();
};
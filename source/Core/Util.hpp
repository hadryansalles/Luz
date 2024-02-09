#pragma once

#include "Base.hpp"

#include <string>
#include <vector>

std::string EncodeBase64(unsigned char const* input, size_t len);
std::vector<u8> DecodeBase64(std::string const& input);

struct TimeScopeImpl;

struct TimeScope {
    std::string title;

    TimeScope(const std::string& title);
    ~TimeScope();
private:
    TimeScopeImpl* impl;
};
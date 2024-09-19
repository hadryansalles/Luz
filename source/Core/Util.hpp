#pragma once

#include "Base.hpp"

#include <string>
#include <vector>

std::string EncodeBase64(unsigned char const* input, size_t len);
std::vector<u8> DecodeBase64(std::string const& input);

u32 HashUUID(const std::vector<UUID>& vec);

template <typename T>
void HashCombine(u32& h, const T& v) {
    std::hash<T> hash;
    h ^= hash(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
}

inline void HashCombine(u32& h, void* ptr, uint32_t size) {
    h = std::hash<std::string_view>()(std::string_view((char*)ptr, size));
}

inline float Halton(uint32_t i, uint32_t b) {
    float f = 1.0f;
    float r = 0.0f;

    while (i > 0) {
        f /= static_cast<float>(b);
        r = r + f * static_cast<float>(i % b);
        i = static_cast<uint32_t>(floorf(static_cast<float>(i) / static_cast<float>(b)));
    }

    return r;
}
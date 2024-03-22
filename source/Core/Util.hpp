#pragma once

#include "Base.hpp"

#include <string>
#include <vector>

std::string EncodeBase64(unsigned char const* input, size_t len);
std::vector<u8> DecodeBase64(std::string const& input);

u32 HashUUID(const std::vector<UUID>& vec);
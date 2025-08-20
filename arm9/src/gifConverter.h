#ifndef GIF_CONVERTER_H
#define GIF_CONVERTER_H

#include <array>
#include <cstdint>
#include <span>

#include "unlaunch.h"

std::span<uint8_t> parseGif(const char* path, std::array<uint8_t, MAX_GIF_SIZE>& outArr, volatile uint16_t* decompressedData);

#endif //GIF_CONVERTER_H

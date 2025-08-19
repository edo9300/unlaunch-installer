#include <array>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <format>
#include <memory>
#include <stdexcept>
#include <vector>

#include "storage.h"
#include "unlaunch.h"

struct Gif {
    struct Header {
        char signature[6];
        uint16_t width;
        uint16_t height;
        uint8_t gctSize: 3;
        uint8_t sortFlag: 1;
        uint8_t colorResolution: 3;
        uint8_t gctFlag: 1;
        uint8_t bgColor;
        uint8_t pixelAspectRatio;
    } __attribute__ ((__packed__)) header;
    static_assert(sizeof(Header) == 13);

    std::array<uint8_t, 255 * 3> colorTable;
    size_t numColors;

    struct Frame {
        struct Descriptor {
            uint16_t x;
            uint16_t y;
            uint16_t w;
            uint16_t h;
            uint8_t lctSize: 3;
            uint8_t reserved: 2;
            uint8_t sortFlag: 1;
            uint8_t interlaceFlag: 1;
            uint8_t lctFlag: 1;
        } __attribute__ ((__packed__)) descriptor;
        static_assert(sizeof(Descriptor) == 9);

        struct Image {
            uint8_t lzwMinimumCodeSize;
            std::vector<std::vector<uint8_t>> imageDataChunks;
        } image;
    } frame;
};

static constexpr auto WIDTH = 256;
static constexpr auto HEIGHT = 192;

Gif getGif(std::string_view path) {
    Gif gif;
    auto file = fopen(path.data(), "rb");
    if (!file)
        throw std::runtime_error("Failed to open file");

    auto fileSptr = std::shared_ptr<FILE>(file, fclose);
	
	if(auto size = getFileSize(file); size < 7 || size > MAX_GIF_SIZE)
	{
		throw std::runtime_error("Gif file too big.\n");
	}

    auto& header = gif.header;

    // Read header
    fread(&header, 1, sizeof(header), file);

    // Check that this is a GIF
    if (memcmp(header.signature, "GIF87a", sizeof(header.signature)) != 0 && memcmp(header.signature, "GIF89a", sizeof(header.signature)) != 0) {
        throw std::runtime_error("File not a gif");
    }

    if(header.width != WIDTH || header.height != HEIGHT) {
        throw std::runtime_error("Invalid gif size");
    }

    auto& numColors = gif.numColors;
    auto& color_table = gif.colorTable;
    // Load global color table
    if (header.gctFlag) {
        numColors = (2 << header.gctSize);
        fread(color_table.data(), 1, numColors * 3, file);
    }

    auto& frame = gif.frame;
    bool gotImage = false;
    while (1) {
        switch (fgetc(file)) {
        case 0x21: { // Extension
            switch (fgetc(file)) {
                case 0xF9: { // Graphics Control
                    uint8_t toRead = fgetc(file);
                    fseek(file, toRead, SEEK_CUR);
                    fgetc(file); // Terminator
                    break;
                }
                case 0x01: { // Plain text
                    throw std::runtime_error("Plain text found");
#if 0
                    fseek(file, 12, SEEK_CUR);
                    while (uint8_t size = fgetc(file)) {
                        fseek(file, size, SEEK_CUR);
                    }
                    break;
#endif
                }
                case 0xFF: { // Application extension
                    throw std::runtime_error("Application extension found");
#if 0
                    if (fgetc(file) == 0xB) {
                        char buffer[0xC] = {0};
                        fread(buffer, 1, 0xB, file);
                        if (strcmp(buffer, "NETSCAPE2.0") == 0) { // Check for Netscape loop count
                            fseek(file, 2, SEEK_CUR);
                            fseek(file, 2, SEEK_CUR);
                            fgetc(file); //terminator
                            break;
                        }
                    }
                    [[fallthrough]];
#endif
                }
                case 0xFE: { // Comment
                    // Skip comments and unsupported application extionsions
                    while (uint8_t size = fgetc(file)) {
                        fseek(file, size, SEEK_CUR);
                    }
                    break;
                }
                default: {
                    throw std::runtime_error("Unknown GIF extension found");
                }
            }
            break;
        } case 0x2C: { // Image descriptor
            gotImage = true;
            fread(&frame.descriptor, 1, sizeof(frame.descriptor), file);
            if (frame.descriptor.lctFlag) {
                header.gctFlag = 1;
                header.gctSize = frame.descriptor.lctSize;
                header.sortFlag = frame.descriptor.sortFlag;
                numColors = 2 << header.gctSize;
                fread(color_table.data(), 1, numColors * 3, file);
                frame.descriptor.lctFlag = 0;
                frame.descriptor.lctSize = 0;
                frame.descriptor.sortFlag = 0;
            }

            if(frame.descriptor.w != WIDTH || frame.descriptor.h != HEIGHT) {
                throw std::runtime_error("Wrong frame size");
            }

            if(frame.descriptor.x != 0 || frame.descriptor.y != 0) {
                throw std::runtime_error("Wrong frame coordinates");
            }

            frame.image.lzwMinimumCodeSize = fgetc(file);
            while (uint8_t size = fgetc(file)) {
                std::vector<uint8_t> dataChunk;
                dataChunk.resize(size);
                fread(dataChunk.data(), 1, size, file);
                frame.image.imageDataChunks.push_back(std::move(dataChunk));
            }

            goto breakWhile;
        } case 0x3B: { // Trailer
            goto breakWhile;
        }
        }
        if(feof(file)){
            throw std::runtime_error("Unexpected file termination");
        }
    }
breakWhile:
    if(!gotImage){
        throw std::runtime_error("Image data not found in gif");
    }
    if(!header.gctFlag) {
        throw std::runtime_error("Invalid gif (missing color table)");
    }
    return gif;
}

std::span<uint8_t> writeGif(const Gif& gif, std::array<uint8_t, MAX_GIF_SIZE>& outArr) {
    size_t totalWritten = 0;
    auto writeArr = [&, ptr = outArr.data()](const void* data, size_t len) mutable {
        if((totalWritten + len) > MAX_GIF_SIZE)
            throw std::runtime_error("Gif too big");
        memcpy(ptr, data, len);
        ptr += len;
        totalWritten += len;
    };
    auto writeCh = [&](char ch) mutable {
        writeArr(&ch, 1);
    };

    writeArr(&gif.header, sizeof(gif.header));
    writeArr(gif.colorTable.data(), gif.numColors * 3);

    // write single image descriptor
    writeCh(0x2C);
    writeArr(&gif.frame.descriptor, sizeof(gif.frame.descriptor));

    // write image data
    writeCh(gif.frame.image.lzwMinimumCodeSize);
    for(const auto& chunk : gif.frame.image.imageDataChunks){
        writeCh(chunk.size());
        writeArr(chunk.data(), chunk.size());
    }
    writeCh('\0');

    // write trailer
    writeCh(0x3B);
    return std::span{outArr.data(), totalWritten};
}


std::span<uint8_t> parseGif(const char* path, std::array<uint8_t, MAX_GIF_SIZE>& outArr) {
	const auto gif = getGif(path);
	return writeGif(gif, outArr);
}

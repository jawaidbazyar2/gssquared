#pragma once

#include <cstdint>
#include <cstdlib>

#include "computer.hpp"

#pragma pack(push, 1)  // Ensure no padding between struct members

// BMP File Header (14 bytes)
struct BMPFileHeader {
    uint16_t signature;      // "BM" (0x4D42)
    uint32_t fileSize;       // Size of the BMP file in bytes
    uint16_t reserved1;      // Reserved field (should be 0)
    uint16_t reserved2;      // Reserved field (should be 0)
    uint32_t dataOffset;     // Offset to start of pixel data
};

// BMP Info Header (40 bytes) - BITMAPINFOHEADER format
struct BMPInfoHeader {
    uint32_t headerSize;     // Size of this header (40 bytes)
    int32_t  width;          // Image width in pixels
    int32_t  height;         // Image height in pixels (positive = bottom-up)
    uint16_t planes;         // Number of color planes (must be 1)
    uint16_t bitsPerPixel;   // Bits per pixel (24 for 24bpp)
    uint32_t compression;    // Compression method (0 = none)
    uint32_t imageSize;      // Size of raw bitmap data (can be 0 for uncompressed)
    int32_t  xPixelsPerMeter; // Horizontal resolution
    int32_t  yPixelsPerMeter; // Vertical resolution
    uint32_t colorsUsed;     // Number of colors in palette (0 for 24bpp)
    uint32_t colorsImportant; // Number of important colors (0 = all)
};

// Complete BMP Header combining both file and info headers
struct BMPHeader {
    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;
    
    // Constructor for 24bpp BMP
    BMPHeader(int32_t width, int32_t height) {
        // Calculate row size (padded to 4-byte boundary)
        int32_t rowSize = ((width * 3 + 3) / 4) * 4; // the formats we'll have will all be multiple of 4 with no padding
        int32_t imageSize = rowSize * abs(height);
        
        // File Header
        fileHeader.signature = 0x4D42;  // "BM"
        fileHeader.fileSize = sizeof(BMPHeader) + imageSize;
        fileHeader.reserved1 = 0;
        fileHeader.reserved2 = 0;
        fileHeader.dataOffset = sizeof(BMPHeader);
        
        // Info Header
        infoHeader.headerSize = sizeof(BMPInfoHeader);
        infoHeader.width = width;
        infoHeader.height = height;
        infoHeader.planes = 1;
        infoHeader.bitsPerPixel = 24;
        infoHeader.compression = 0;  // No compression
        infoHeader.imageSize = imageSize;
        infoHeader.xPixelsPerMeter = 2835;  // ~72 DPI
        infoHeader.yPixelsPerMeter = 2835;  // ~72 DPI
        infoHeader.colorsUsed = 0;
        infoHeader.colorsImportant = 0;
    }
};

#pragma pack(pop)  // Restore default packing


struct ClipboardImage {
    BMPHeader *header = nullptr;

    uint8_t *clip_buffer = nullptr;


    ClipboardImage();
    ~ClipboardImage();
    void Clip(SDL_Surface *surface);
};


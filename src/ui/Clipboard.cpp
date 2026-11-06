#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

#include <SDL3/SDL_clipboard.h>

#include "computer.hpp"
#include "display/display.hpp"
#include "Clipboard.hpp"

#define MAX_CLIP_WIDTH (768)
#define MAX_CLIP_HEIGHT (256*2)

// make it big enough to handle shr with borders
ClipboardImage::ClipboardImage( ) {
    uint32_t clip_data_size = MAX_CLIP_WIDTH * MAX_CLIP_HEIGHT * 3;
    clip_buffer = new uint8_t[sizeof(BMPHeader) + clip_data_size];
    header = nullptr;
}

ClipboardImage::~ClipboardImage() {
    delete[] clip_buffer;
    if (header != nullptr) {
        delete header;
        header = nullptr;
    }
}

const void *clip_callback(void *userdata, const char *mime_type, size_t *size) {
    // TODO: for now just assume BMP only
    
    ClipboardImage *clip = (ClipboardImage *)userdata;
    uint32_t bytes_per_line = clip->header->infoHeader.width * 3;
    // Round bytes_per_line up to the next multiple of 4
    if (bytes_per_line % 4 != 0) {
        bytes_per_line = ((bytes_per_line + 3) / 4) * 4;
    }

    size_t calcsize = sizeof(BMPHeader) + (bytes_per_line * (clip->header->infoHeader.height) );
    //printf("clip_callback: %s (%d)\n", mime_type, calcsize);
    *size = calcsize;
    return clip->clip_buffer;
}

#if 0
void ClipboardImage::Clip(computer_t *computer) {
    uint32_t width, height;
    const char *mime_types[] = { "image/bmp" };

    //display_engine_get_buffer(uint8_t *buffer, uint32_t *width, uint32_t *height);
    display_engine_get_buffer(computer, clip_buffer + sizeof(BMPHeader), &width, &height);

    if (header != nullptr) {
        delete header;
        header = nullptr;
    }
    header = new BMPHeader(width, height);
    memcpy(clip_buffer, header, sizeof(BMPHeader));
    bool res = SDL_SetClipboardData(clip_callback, nullptr, (void *)this, mime_types, 1 );
    if (!res) {
        printf("Failed to set clipboard data: %s\n", SDL_GetError());
    }
}
#endif

void ClipboardImage::Clip(SDL_Surface *surface, bool double_scanlines) {
    if (!surface || surface->w <= 0 || surface->h <= 0) return;
    const int repeats = double_scanlines ? 2 : 1;
    const int width = surface->w;
    const int height = surface->h;
    if (width > std::numeric_limits<int32_t>::max() / 4 ||
        height > std::numeric_limits<int32_t>::max() / repeats) return;
    const size_t row_bytes = static_cast<size_t>(width) * 3;
    const size_t stride = (row_bytes + 3) & ~size_t(3);
    const size_t out_height = static_cast<size_t>(height) * repeats;
    if (stride > static_cast<size_t>(std::numeric_limits<int32_t>::max()) / out_height) return;
    const size_t image_bytes = stride * out_height;
    if (image_bytes > std::numeric_limits<uint32_t>::max() - sizeof(BMPHeader)) return;

    SDL_Surface *converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    if (!converted) return;
    auto *new_buffer = new (std::nothrow) uint8_t[sizeof(BMPHeader) + image_bytes];
    auto *new_header = new (std::nothrow) BMPHeader(width, static_cast<int32_t>(out_height));
    if (!new_buffer || !new_header) {
        delete[] new_buffer;
        delete new_header;
        SDL_DestroySurface(converted);
        return;
    }
    std::memcpy(new_buffer, new_header, sizeof(BMPHeader));
    uint8_t *dst = new_buffer + sizeof(BMPHeader);
    // BMP is bottom-up, with BGR pixels and each row padded to four bytes.
    for (int y = height - 1; y >= 0; --y) {
        const auto *src = static_cast<const uint8_t *>(converted->pixels) + static_cast<size_t>(y) * converted->pitch;
        for (int copy = 0; copy < repeats; ++copy) {
            for (int x = 0; x < width; ++x) {
                *dst++ = src[x * 4 + 2];
                *dst++ = src[x * 4 + 1];
                *dst++ = src[x * 4];
            }
            std::memset(dst, 0, stride - row_bytes);
            dst += stride - row_bytes;
        }
    }
    SDL_DestroySurface(converted);

    uint8_t *old_buffer = clip_buffer;
    BMPHeader *old_header = header;
    clip_buffer = new_buffer;
    header = new_header;
    const char *mime_types[] = { "image/bmp" };
    if (!SDL_SetClipboardData(clip_callback, nullptr, this, mime_types, 1)) {
        clip_buffer = old_buffer;
        header = old_header;
        delete[] new_buffer;
        delete new_header;
        printf("Failed to set clipboard data: %s\n", SDL_GetError());
        return;
    }
    delete[] old_buffer;
    delete old_header;
}

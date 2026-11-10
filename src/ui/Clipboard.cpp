#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL_clipboard.h>

#include "computer.hpp"
#include "display/display.hpp"
#include "Clipboard.hpp"

#define MAX_CLIP_WIDTH (768)
#define MAX_CLIP_HEIGHT (256*2)

// make it big enough to handle shr with borders
ClipboardImage::ClipboardImage( ) {
    uint32_t clip_data_size = MAX_CLIP_WIDTH * MAX_CLIP_HEIGHT * 3;
    clip_buffer_size = sizeof(BMPHeader) + clip_data_size;
    clip_buffer = new uint8_t[clip_buffer_size];
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

    // pass back the size.
    uint32_t width = surface->w;
    uint32_t height = surface->h;
    int copies = double_scanlines ? 2 : 1;

    // A row of 3-byte pixels needs width % 4 bytes to reach a multiple of 4.
    uint32_t lineextra = width % 4;

    // A card monitor's picture can be larger than the emulated display.
    size_t needed = sizeof(BMPHeader) + (size_t)(width * 3 + lineextra) * height * copies;
    if (needed > clip_buffer_size) {
        delete[] clip_buffer;
        clip_buffer = new uint8_t[needed];
        clip_buffer_size = needed;
    }

    // BMP files have the last scanline first. What? 
    // Copy RGB values without alpha channel
    //RGBA_t *src = (RGBA_t *)ds->buffer;
    uint8_t *dst = clip_buffer + sizeof(BMPHeader); // start past BPHeader

    for (int scanline = height - 1; scanline >= 0; scanline--) {
        for (int doub = 0; doub < copies; doub++) {
            for (int i = 0; i < width; i++) {
                RGBA_t *pix = (RGBA_t *)surface->pixels + (scanline * width + i);
                //RGBA_t pix = { .r = pixel[0], .g = pixel[1], .b = pixel[2], .a = pixel[3] };
                *dst++ = pix->b;  // TODO: this order may be platform-dependent. works on mac and windows so far..
                *dst++ = pix->g;
                *dst++ = pix->r;
            }
            // pad out to the next multiple of 4 bytes.
            for (int i = 0; i < lineextra; i++) {
                *dst++ = 0;
            }
        }
        
    }

    const char *mime_types[] = { "image/bmp" };

    if (header != nullptr) {
        delete header;
        header = nullptr;
    }
    header = new BMPHeader(width, height*copies);
    memcpy(clip_buffer, header, sizeof(BMPHeader));
    bool res = SDL_SetClipboardData(clip_callback, nullptr, (void *)this, mime_types, 1 );
    if (!res) {
        printf("Failed to set clipboard data: %s\n", SDL_GetError());
    }
}
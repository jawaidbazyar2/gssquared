#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <SDL3/SDL.h>

struct color_mode_t {
    /* uint8_t colorburst;
    uint8_t mixed_mode;
    uint8_t phase_offset;
    uint8_t unused; */
    uint8_t colorburst:1;
    uint8_t mixed_mode:1;
    uint8_t phase_offset:1;

};

template<typename T>
class MemoryStorage {
    private:
        T* ptr;
    public:
        MemoryStorage() {}
        ~MemoryStorage() { free(ptr); }

        T* allocate(size_t width, size_t height, size_t size) {
            printf("Allocating %zu bytes for [%zu][%zu] x %zu\n", size * height * width, size, width, height);
            size_t total_size = size * height * width;
            size_t aligned_size = (total_size + 63) & ~63;
            ptr = static_cast<T*>(aligned_alloc(64, aligned_size));
            return ptr;
        }
        
        void* lock() { return nullptr; } // No-op for memory
        void unlock() { } // No-op for memory
};

class SDLTextureStorage {
    SDL_Texture* texture;
public:
    SDLTextureStorage(int w, int h, SDL_Renderer* renderer, SDL_PixelFormat format) {
        printf("Creating texture %d x %d %08X\n", w, h, format);
        texture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STREAMING, w, h);
        if (!texture) {
            throw std::runtime_error("Failed to create texture");
        }
    }

    ~SDLTextureStorage() {
        SDL_DestroyTexture(texture);
    }
    
    template<typename T>
    T* allocate(size_t width, size_t height) { return nullptr; } // Texture provides memory
    
    void* lock() {
        void* pixels; int pitch;
        SDL_LockTexture(texture, nullptr, &pixels, &pitch);
        return pixels;
    }
    
    void unlock() { SDL_UnlockTexture(texture); }
    SDL_Texture* get_texture() { return texture; }
};

template<typename bs_t, uint32_t HEIGHT, uint32_t WIDTH, typename StoragePolicy = MemoryStorage<bs_t>>
class Frame {
private:
    uint32_t hloc;
    uint32_t scanline;
    uint32_t f_width; // purely informational, for consumers
    uint32_t f_height;
    
    StoragePolicy *storage;
    bs_t (* __restrict stream)[WIDTH];
    bs_t* __restrict row;
    SDL_Texture* __restrict texture;
    color_mode_t line_mode[HEIGHT];

public:
    //Frame(uint16_t width, uint16_t height);  // pixels

    Frame(uint32_t width, uint32_t height, SDL_Renderer* renderer = nullptr, SDL_PixelFormat format = SDL_PIXELFORMAT_UNKNOWN);   

    ~Frame() { 
        delete storage;
    }

    void print() {
        if (texture != nullptr) return;
    
        printf("Frame: %u x %u\n", WIDTH, HEIGHT);
        for (size_t i = 0; i < HEIGHT; i++) {
            set_line(i);
            size_t linepos = 0;
            for (size_t j = 0; j < WIDTH / 64; j++) {
                uint64_t wrd = 0;
                for (size_t b = 0; b < 64; b++) {
                    wrd = (wrd << 1) | (pull()&1);
                    if (linepos++ >= WIDTH) break; // don't go past the end of the line
                }
                printf("%016llx ", wrd);
                if (linepos >= WIDTH) break; // don't go past the end of the line
            }
            printf("\n");
        }
    }
    inline bs_t *data() {
        return stream[0];
    }

    inline void push(bs_t bit) noexcept { 
        //stream[scanline][hloc++] = bit;
        row[hloc++] = bit;
    }

    inline bs_t pull() noexcept { 
        //return stream[scanline][hloc++];
        return row[hloc++];
    }

    inline bs_t peek() noexcept {
        //return stream[scanline][hloc];
        return row[hloc];
    }
    
    inline void set_line(int line) noexcept { 
        scanline = line;
        row = stream[scanline];
        hloc = 0;
    }

    inline void open() {
        if constexpr (std::is_same_v<StoragePolicy, SDLTextureStorage>) {
            void *pixels;
            int pitch;
    
            SDL_LockTexture(texture, nullptr, &pixels, &pitch);
            stream = static_cast<bs_t(*)[WIDTH]>(pixels);
        }
    }

    inline void close() {
        if constexpr (std::is_same_v<StoragePolicy, SDLTextureStorage>) {
            SDL_UnlockTexture(texture);
        }
    }

    inline SDL_Texture* get_texture() { return texture; }

    inline void set_color_mode(uint32_t line, color_mode_t mode) {
        if (line >= HEIGHT) {
            printf("set_color_mode: line out of bounds: %d\n", line);
            return;
        }
        line_mode[line] = mode;
    }

    inline color_mode_t get_color_mode(uint32_t line) {
        return line_mode[line];
    }

    inline uint32_t width() { return f_width; }
    inline uint32_t height() { return f_height; }
    //void clear(bs_t clr = {0});
    
    void clear(bs_t clr) {
        if (texture != nullptr) return;
    
        for (size_t i = 0; i < HEIGHT; ++i) {
            for (size_t j = 0; j < WIDTH; ++j) {
                stream[i][j] = clr;
            }
        }
        scanline = 0;
        hloc = 0;
    }

    // Getters for template parameters
    static constexpr uint32_t max_width() { return WIDTH; }
    static constexpr uint32_t max_height() { return HEIGHT; }
};

template<typename bs_t, uint32_t HEIGHT, uint32_t WIDTH, typename StoragePolicy>
Frame<bs_t, HEIGHT, WIDTH, StoragePolicy>::Frame(uint32_t width, uint32_t height, SDL_Renderer* renderer, SDL_PixelFormat format)
: f_width(width), f_height(height), scanline(0), hloc(0) 
{
    texture = nullptr;

    // read texture dimensions and ensure they match the frame dimensions
    if constexpr (std::is_same_v<StoragePolicy, SDLTextureStorage>) {

        storage = new StoragePolicy(width, height, renderer, format);
        texture = storage->get_texture();
        
        stream = nullptr; // Will be set on lock

    } else if constexpr (std::is_same_v<StoragePolicy, MemoryStorage<bs_t>>) {
        texture = nullptr;
        // Memory storage path
        storage = new StoragePolicy();
        stream = (bs_t (*)[WIDTH]) storage->allocate(WIDTH, HEIGHT, sizeof(bs_t));
        if (stream == nullptr) {
            throw std::bad_alloc();
        }

    /*             // Calculate size and ensure it's a multiple of alignment (64 bytes)
        size_t total_size = sizeof(bs_t) * HEIGHT * WIDTH;
        size_t aligned_size = (total_size + 63) & ~63;  // Round up to multiple of 64
        
        stream = static_cast<bs_t(*)[WIDTH]>(
            aligned_alloc(64, aligned_size)
        );
        
        if (stream == nullptr) {
            fprintf(stderr, "Frame allocation failed: requested %zu bytes (aligned to %zu)\n", 
                    total_size, aligned_size);
            throw std::bad_alloc();
        } */
    } else {
        throw std::invalid_argument("Invalid storage policy");
    }
    for (size_t i = 0; i < HEIGHT; i++) {
        line_mode[i] = {0, 0};
    }
}
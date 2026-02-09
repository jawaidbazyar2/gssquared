#pragma once

/* This is a dirty hack to work around the fact that on linux 
    uint64_t is unsigned long long and on MacOS it is unsigned long, and clang-linux "helpfully"
    warns if you try to use %llu on a uint64_t.
*/

template<typename T>
inline unsigned long long u64_t(T value) {
    static_assert(sizeof(T) == 8, "Type must be 64-bit");
    return static_cast<unsigned long long>(value);
}

#pragma once

template<typename T>
inline unsigned long long u64_t(T value) {
    static_assert(sizeof(T) == 8, "Type must be 64-bit");
    return static_cast<unsigned long long>(value);
}

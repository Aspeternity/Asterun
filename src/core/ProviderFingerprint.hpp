#pragma once

#include <cstdint>
#include <string_view>

namespace altrun::fingerprint {

inline constexpr std::uint64_t kOffset =
    1469598103934665603ull;
inline constexpr std::uint64_t kPrime =
    1099511628211ull;

inline void MixByte(
    std::uint64_t& hash,
    std::uint8_t value) noexcept {

    hash ^= value;
    hash *= kPrime;
}

inline void Mix(
    std::uint64_t& hash,
    std::wstring_view value) noexcept {

    for (wchar_t ch : value) {
        const auto code =
            static_cast<std::uint32_t>(ch);

        MixByte(
            hash,
            static_cast<std::uint8_t>(
                code & 0xffu));
        MixByte(
            hash,
            static_cast<std::uint8_t>(
                (code >> 8u) & 0xffu));
        MixByte(
            hash,
            static_cast<std::uint8_t>(
                (code >> 16u) & 0xffu));
        MixByte(
            hash,
            static_cast<std::uint8_t>(
                (code >> 24u) & 0xffu));
    }
}

inline void Mix(
    std::uint64_t& hash,
    std::string_view value) noexcept {

    for (unsigned char ch : value) {
        MixByte(hash, ch);
    }
}

inline void Mix(
    std::uint64_t& hash,
    std::uint64_t value) noexcept {

    for (int shift = 0;
         shift < 64;
         shift += 8) {
        MixByte(
            hash,
            static_cast<std::uint8_t>(
                (value >> shift) &
                0xffu));
    }
}

inline std::uint64_t Hash(
    std::wstring_view value) noexcept {

    std::uint64_t hash = kOffset;
    Mix(hash, value);
    return hash;
}

inline std::uint64_t Hash(
    std::string_view value) noexcept {

    std::uint64_t hash = kOffset;
    Mix(hash, value);
    return hash;
}

} // namespace altrun::fingerprint

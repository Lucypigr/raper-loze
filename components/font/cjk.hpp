// SPDX-License-Identifier: GPL-3.0-or-later
// D2Decomp — bitmap glyphs for the characters D2's fonts lack.
//
// D2's LATIN fonts stop at 0xFF, and an English install has no CHI fonts,
// so Chinese text (a CHI string table, tbl::kWide in the game's strings)
// draws from this instead: assets/fonts/cubic11.cjk, the Cubic 11 pixel
// font packed by assets/fonts/make_cjk_font.py. Font::draw falls back to it
// for every character past 0xFF.
//
// Format (little-endian): "D2CJK1\0\0", u8 height, u8 row_bytes, u32 count,
// then count records sorted by code: u16 code, u8 advance, height * row_bytes
// bitmap bytes (rows top-down, MSB = leftmost pixel).
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace d2d::font {

class CjkFont {
public:
    struct Glyph {
        std::uint16_t code{};
        std::uint8_t  advance{};
        std::size_t   bitmap{};   // offset of the rows in bytes_
    };

    CjkFont() = default;

    explicit CjkFont(std::span<const std::uint8_t> bytes) : bytes_(bytes.begin(), bytes.end()) {
        constexpr std::size_t kHeader = 14;
        if (bytes_.size() < kHeader || std::memcmp(bytes_.data(), "D2CJK1\0\0", 8) != 0)
            throw std::runtime_error("cjk font: bad magic");
        height_ = bytes_[8];
        row_bytes_ = bytes_[9];
        std::uint32_t count = 0;
        std::memcpy(&count, bytes_.data() + 10, 4);
        const std::size_t record = 3 + std::size_t(height_) * row_bytes_;
        if (row_bytes_ == 0 || row_bytes_ > 4 || bytes_.size() < kHeader + record * count)
            throw std::runtime_error("cjk font: truncated");
        glyphs_.reserve(count);
        for (std::size_t at = kHeader; glyphs_.size() < count; at += record)
            glyphs_.push_back({ std::uint16_t(bytes_[at] | (bytes_[at + 1] << 8)), bytes_[at + 2], at + 3 });
    }

    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] std::size_t glyph_count() const noexcept { return glyphs_.size(); }

    // Nullptr when the font hasn't the character.
    [[nodiscard]] const Glyph* find(std::uint16_t code) const {
        const auto found = std::ranges::lower_bound(glyphs_, code, {}, &Glyph::code);
        return found != glyphs_.end() && found->code == code ? &*found : nullptr;
    }

    [[nodiscard]] bool pixel(const Glyph& glyph, int x, int y) const {
        if (x < 0 || y < 0 || x >= row_bytes_ * 8 || y >= height_) return false;
        const auto byte = bytes_[glyph.bitmap + std::size_t(y) * row_bytes_ + std::size_t(x / 8)];
        return (byte & (0x80 >> (x % 8))) != 0;
    }

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<Glyph> glyphs_;
    int height_ = 0;
    int row_bytes_ = 0;
};

// The glyphs Font draws characters past 0xFF from; none until the client
// sets them (then English text draws exactly as before).
inline const CjkFont*& cjk_fallback() {
    static const CjkFont* fallback = nullptr;
    return fallback;
}

}  // namespace d2d::font

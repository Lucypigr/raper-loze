// SPDX-License-Identifier: GPL-3.0-or-later
// D2Decomp TBL string-table parser.
//
// Format (Phrozen Keep spec, confirmed by our RE of D2 1.14d's
// .\StrTable\strtable.cpp): a 21-byte packed header, an indices array
// (u16 * NodesNumber, unused for lookup — hash-mapping metadata), a hash
// node array (17 packed bytes each), then the string data blob. Keys are
// NUL-terminated ASCII, values NUL-terminated UTF-16LE.
//
// We ignore the on-disk hash structure and rebuild a std::unordered_map
// at load time — TBLs are small (largest is ~400 KB), map lookup is O(1),
// and the on-disk hash is designed for zero-alloc in-place scans we don't
// need. Load once at startup, keep forever.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace d2d::tbl {

class Table {
public:
    Table() = default;

    explicit Table(std::span<const std::byte> bytes) {
        parse(bytes);
    }

    [[nodiscard]] std::optional<std::u16string_view>
    get(std::string_view key) const {
        // heterogeneous lookup would remove the string(), but not worth the
        // template ceremony until profiling asks for it.
        if (auto found = entries_.find(std::string(key)); found != entries_.end()) {
            return std::u16string_view(found->second);
        }
        return std::nullopt;
    }

    // Lookup by the 16-bit `Index` field of the hash node. D2's game.exe
    // stores that ID directly in menu/UI record fields (e.g. the front-end
    // menu table at 0x00708ec0+ carries button labels as IDs 0x13f2..).
    // See docs/research/re/frontend-menu-table.md for the anchor use.
    [[nodiscard]] std::optional<std::u16string_view>
    get(std::uint16_t id) const {
        if (auto found = by_id_.find(id); found != by_id_.end()) {
            return std::u16string_view(found->second);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

    // Direct map access for diagnostics / bulk scans. Not intended for hot
    // paths — use get(key) or get(id).
    [[nodiscard]] const auto& entries() const noexcept { return entries_; }

private:
    static std::uint32_t rd32(const std::byte* source) {
        std::uint32_t value; std::memcpy(&value, source, 4); return value;
    }
    static std::uint16_t rd16(const std::byte* source) {
        std::uint16_t value; std::memcpy(&value, source, 2); return value;
    }

    // UTF-8 (BMP: up to three bytes) into `out`. False, `out` untouched,
    // when `raw` isn't UTF-8 or is plain ASCII (Latin-1 reads it the same).
    static bool decode_utf8(std::span<const unsigned char> raw, std::u16string& out) {
        std::u16string text;
        text.reserve(raw.size());
        bool multi_byte = false;
        for (std::size_t at = 0; at < raw.size();) {
            const unsigned char lead = raw[at];
            int extra = 0;
            char32_t code = lead;
            if (lead >= 0xE0 && lead < 0xF0) { extra = 2; code = lead & 0x0F; }
            else if (lead >= 0xC2 && lead < 0xE0) { extra = 1; code = lead & 0x1F; }
            else if (lead >= 0x80) return false;
            if (at + std::size_t(extra) >= raw.size() && extra > 0) return false;   // cut short
            for (int i = 1; i <= extra; ++i) {
                const unsigned char next = raw[at + std::size_t(i)];
                if ((next & 0xC0) != 0x80) return false;
                code = (code << 6) | (next & 0x3F);
            }
            if (extra == 2 && code < 0x800) return false;   // overlong
            multi_byte |= extra > 0;
            text.push_back(char16_t(code));
            at += std::size_t(extra) + 1;
        }
        if (!multi_byte) return false;
        out = std::move(text);
        return true;
    }

    void parse(std::span<const std::byte> bytes) {
        constexpr std::size_t kHeader = 21;
        constexpr std::size_t kNode   = 17;
        if (bytes.size() < kHeader) throw std::runtime_error("TBL: truncated header");

        const auto nodesNumber   = rd16(bytes.data() + 0x02);
        const auto hashTableSize = rd32(bytes.data() + 0x04);
        // +0x08 Version — 0 or 1, don't care.
        // +0x09 DataStartOffset, +0x0D HashMaxTries, +0x11 FileSize — unused.

        const std::size_t nodesOff = kHeader + std::size_t(nodesNumber) * 2;
        const std::size_t stringsOff = nodesOff
                                     + std::size_t(hashTableSize) * kNode;
        if (bytes.size() < stringsOff) throw std::runtime_error("TBL: truncated body");

        entries_.reserve(hashTableSize);
        for (std::uint32_t i = 0; i < hashTableSize; ++i) {
            const std::byte* node = bytes.data() + nodesOff + i * kNode;
            if (std::uint8_t(node[0]) == 0) continue;   // deleted entry
            const auto keyOff = rd32(node + 0x07);
            const auto valOff = rd32(node + 0x0b);
            const auto valLen = rd16(node + 0x0f);        // includes NUL, in u16 chars

            if (keyOff >= bytes.size() || valOff >= bytes.size()) continue;

            const auto* keyp = reinterpret_cast<const char*>(bytes.data() + keyOff);
            const std::size_t keyMax = bytes.size() - keyOff;
            std::size_t keyLen = 0;
            while (keyLen < keyMax && keyp[keyLen] != '\0') ++keyLen;

            // D2 stores values as single-byte (Latin-1 / Windows-1252 for
            // LATIN builds; a locale-specific MBCS for CYR/JPN/KOR). The
            // Chinese (CHI) tables 1.14d-era text patches ship are UTF-8,
            // "ÿc" colour codes included: a value that is valid UTF-8 with a
            // multi-byte sequence decodes as UTF-8, any other as Latin-1, so
            // the English tables read as before. unverified (source:
            // OpenD2's Latin path): the LATIN encoding.
            if (valLen == 0 || valOff + std::size_t(valLen) > bytes.size()) continue;
            const auto* valp = reinterpret_cast<const unsigned char*>(bytes.data() + valOff);
            const std::span<const unsigned char> raw(valp, std::size_t(valLen - 1));   // skip trailing NUL
            std::u16string value;
            if (!decode_utf8(raw, value)) value.assign(raw.begin(), raw.end());

            // Nodes carry a 16-bit `Index` at offset +0x01 — that's D2's ID.
            // Stash the value under it too so callers can look up by either.
            const auto id = rd16(node + 0x01);
            entries_.emplace(std::string(keyp, keyLen), value);
            by_id_.emplace(id, std::move(value));
        }
    }

    std::unordered_map<std::string, std::u16string>   entries_;
    std::unordered_map<std::uint16_t, std::u16string> by_id_;
};

// The game's text strings are Latin-1, one byte a character, as D2's own
// fonts draw them. A character past 0xFF (a Chinese table's) goes in as
// kWide and three bytes 0x80 | six bits, high first: never NUL, '\n' or ' ',
// so splitting lines and words leaves it whole, and it only means a wide
// character after kWide (the Latin-1 conversion drops control codes).
inline constexpr char kWide = '\x01';

inline void append_char(std::string& text, char16_t code) {
    if (code <= 0xFF) { text.push_back(char(code)); return; }
    text.push_back(kWide);
    text.push_back(char(0x80 | (code >> 12)));
    text.push_back(char(0x80 | ((code >> 6) & 0x3F)));
    text.push_back(char(0x80 | (code & 0x3F)));
}

// The character at `at`, stepping past it. A kWide cut short reads as 0.
inline char16_t next_char(std::string_view text, std::size_t& at) {
    const auto lead = static_cast<unsigned char>(text[at++]);
    if (lead != static_cast<unsigned char>(kWide)) return lead;
    if (at + 3 > text.size()) { at = text.size(); return 0; }
    const auto byte = [&](std::size_t i) { return char16_t(static_cast<unsigned char>(text[at + i]) & 0x3F); };
    const auto code = char16_t((byte(0) << 12) | (byte(1) << 6) | byte(2));
    at += 3;
    return code;
}

}  // namespace d2d::tbl

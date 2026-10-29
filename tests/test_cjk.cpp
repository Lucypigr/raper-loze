// SPDX-License-Identifier: GPL-3.0-or-later
// Chinese text without game data: a UTF-8 (CHI) string table decodes, an
// English one still reads as Latin-1, the game's kWide marking round-trips,
// and Cubic 11's glyphs (assets/fonts/cubic11.cjk) load and draw.
#include <cjk.hpp>
#include <tbl.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

// A one-entry .tbl: header, one index, one hash node, then key and value.
std::vector<std::byte> one_entry_tbl(std::string_view key, std::span<const unsigned char> value) {
    constexpr std::size_t kHeader = 21, kNode = 17;
    const std::size_t key_at = kHeader + 2 + kNode;
    const std::size_t value_at = key_at + key.size() + 1;
    std::vector<std::byte> bytes(value_at + value.size() + 1);
    const auto put16 = [&](std::size_t offset, std::uint16_t number) { std::memcpy(&bytes[offset], &number, 2); };
    const auto put32 = [&](std::size_t offset, std::uint32_t number) { std::memcpy(&bytes[offset], &number, 4); };
    put16(0x02, 1);                      // nodes
    put32(0x04, 1);                      // hash table size
    const std::size_t node = kHeader + 2;
    bytes[node] = std::byte{1};          // in use
    put16(node + 0x01, 7);               // Index
    put32(node + 0x07, std::uint32_t(key_at));
    put32(node + 0x0b, std::uint32_t(value_at));
    put16(node + 0x0f, std::uint16_t(value.size() + 1));
    std::memcpy(&bytes[key_at], key.data(), key.size());
    std::memcpy(&bytes[value_at], value.data(), value.size());
    return bytes;
}

// The value's code units, for a failed comparison's message.
bool same(std::optional<std::u16string_view> value, std::u16string_view expected) {
    if (value && *value == expected) return true;
    std::fprintf(stderr, "got:");
    if (value) for (const char16_t unit : *value) std::fprintf(stderr, " %04x", unsigned(unit));
    std::fprintf(stderr, "\n");
    return false;
}

}  // namespace

int main() try {
    // A CHI table's value: UTF-8, its "ÿc" colour code too (ÿc1 then 取消).
    static constexpr unsigned char kChinese[] = { 0xc3, 0xbf, 'c', '1', 0xe5, 0x8f, 0x96, 0xe6, 0xb6, 0x88 };
    static constexpr char16_t kChineseText[] = { 0x00ff, 'c', '1', 0x53d6, 0x6d88 };
    const std::u16string_view chinese_text(kChineseText, std::size(kChineseText));
    const d2d::tbl::Table chinese(one_entry_tbl("cancel", kChinese));
    assert(same(chinese.get("cancel"), chinese_text));
    assert(same(chinese.get(std::uint16_t{7}), chinese_text));

    // An English one's Latin-1 bytes stay one character each, é included.
    static constexpr unsigned char kEnglish[] = { 'C', 'a', 'f', 0xe9 };
    static constexpr char16_t kEnglishText[] = { 'C', 'a', 'f', 0x00e9 };
    const d2d::tbl::Table english(one_entry_tbl("cafe", kEnglish));
    assert(same(english.get("cafe"), std::u16string_view(kEnglishText, std::size(kEnglishText))));

    // The game's strings: Latin-1 as before, 取 as kWide + three bytes.
    static constexpr char16_t kMixed[] = { 0x00ff, 'c', '1', 0x53d6, ' ', 'A' };
    const std::u16string_view mixed(kMixed, std::size(kMixed));
    std::string text;
    for (const char16_t code : mixed) d2d::tbl::append_char(text, code);
    assert(text.size() == 3 + 4 + 2);
    assert(static_cast<unsigned char>(text[0]) == 0xff && text.substr(1, 2) == "c1");
    assert(text.find('\n') == std::string::npos && text.find(' ') == 7);
    std::u16string back;
    for (std::size_t offset = 0; offset < text.size();) back.push_back(d2d::tbl::next_char(text, offset));
    assert(back == mixed);
    std::size_t short_at = 0;   // a kWide cut short (a truncated label) reads as nothing
    assert(d2d::tbl::next_char(std::string(1, d2d::tbl::kWide) + "\x80", short_at) == 0 && short_at == 2);

    // Cubic 11: every glyph a Chinese table needs most, drawn in pixels.
    std::ifstream file(D2D_CJK_FONT, std::ios::binary);
    const std::vector<std::uint8_t> bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    const d2d::font::CjkFont font(bytes);
    std::printf("cubic11.cjk: %zu glyphs, %d px\n", font.glyph_count(), font.height());
    assert(font.glyph_count() > 9000 && font.height() == 12);
    static constexpr char16_t kCommon[] = { 0x53d6, 0x6d88, 0x6697, 0x9ed1, 0x7834, 0x58de, 0x795e, 0xff0c, 0x3002 };   // 取消暗黑破壞神，。
    for (const char16_t code : kCommon) {
        const auto* glyph = font.find(code);
        assert(glyph && glyph->advance > 0);
        int inked = 0;
        for (int y = 0; y < font.height(); ++y)
            for (int x = 0; x < 16; ++x) inked += font.pixel(*glyph, x, y);
        assert(inked > 0);
    }
    assert(!font.find(u'A'));    // Latin stays with D2's own fonts
    std::printf("ok\n");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
}

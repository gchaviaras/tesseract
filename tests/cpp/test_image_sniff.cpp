#include <catch2/catch_test_macros.hpp>

#include "tk/image_sniff.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{

std::vector<std::uint8_t> bytes_of(const std::string& s)
{
    return {s.begin(), s.end()};
}

// RIFF....WEBPVP8X<size=10><flags><reserved...>
std::vector<std::uint8_t> webp_vp8x(std::uint8_t flags)
{
    std::string s = "RIFF\x1a\0\0\0WEBPVP8X\x0a\0\0\0";
    s.assign("RIFF\x1a\x00\x00\x00WEBPVP8X\x0a\x00\x00\x00", 20);
    s.push_back(static_cast<char>(flags));
    s.append(9, '\0');
    return bytes_of(s);
}

std::vector<std::uint8_t> png_with_chunks(const std::vector<std::string>& types)
{
    std::string s("\x89PNG\r\n\x1a\n", 8);
    for (const auto& t : types)
    {
        s.append("\x00\x00\x00\x04", 4); // 4 data bytes
        s += t;
        s.append(4, '\0'); // data
        s.append(4, '\0'); // crc
    }
    return bytes_of(s);
}

} // namespace

TEST_CASE("bytes_may_be_animated: GIF", "[image-sniff]")
{
    CHECK(tk::bytes_may_be_animated(bytes_of("GIF89a\x01\x00\x01\x00")));
    CHECK(tk::bytes_may_be_animated(bytes_of("GIF87a")));
}

TEST_CASE("bytes_may_be_animated: WebP honours the VP8X animation flag", "[image-sniff]")
{
    CHECK(tk::bytes_may_be_animated(webp_vp8x(0x02)));
    CHECK(tk::bytes_may_be_animated(webp_vp8x(0x12))); // animation + alpha
    CHECK_FALSE(tk::bytes_may_be_animated(webp_vp8x(0x10))); // alpha only
    CHECK_FALSE(tk::bytes_may_be_animated(bytes_of(std::string("RIFF\x1a\x00\x00\x00WEBPVP8 ", 16))));
}

TEST_CASE("bytes_may_be_animated: APNG needs acTL before IDAT", "[image-sniff]")
{
    CHECK(tk::bytes_may_be_animated(png_with_chunks({"IHDR", "acTL", "IDAT", "IEND"})));
    CHECK_FALSE(tk::bytes_may_be_animated(png_with_chunks({"IHDR", "IDAT", "IEND"})));
    // acTL after the image data does not make it an APNG.
    CHECK_FALSE(tk::bytes_may_be_animated(png_with_chunks({"IHDR", "IDAT", "acTL"})));
}

TEST_CASE("bytes_may_be_animated: other and truncated input is false", "[image-sniff]")
{
    CHECK_FALSE(tk::bytes_may_be_animated({}));
    CHECK_FALSE(tk::bytes_may_be_animated(bytes_of("\xff\xd8\xff\xe0JFIF")));
    CHECK_FALSE(tk::bytes_may_be_animated(bytes_of("GI")));
    CHECK_FALSE(tk::bytes_may_be_animated(bytes_of("RIFF")));
    CHECK_FALSE(tk::bytes_may_be_animated(bytes_of(std::string("\x89PNG\r\n\x1a\n", 8))));
}

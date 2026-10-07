#include "tk/image_sniff.h"

#include <cstddef>
#include <cstring>

namespace tk
{

namespace
{

bool has_prefix(std::span<const std::uint8_t> b, const char* p, std::size_t at = 0)
{
    const std::size_t n = std::strlen(p);
    return b.size() >= at + n && std::memcmp(b.data() + at, p, n) == 0;
}

bool png_has_actl_before_idat(std::span<const std::uint8_t> b)
{
    std::size_t pos = 8; // after the 8-byte signature
    while (pos + 8 <= b.size())
    {
        const std::uint32_t len = (std::uint32_t{b[pos]} << 24) | (std::uint32_t{b[pos + 1]} << 16) |
                                  (std::uint32_t{b[pos + 2]} << 8) | std::uint32_t{b[pos + 3]};
        if (has_prefix(b, "acTL", pos + 4))
            return true;
        if (has_prefix(b, "IDAT", pos + 4) || has_prefix(b, "IEND", pos + 4))
            return false;
        const std::size_t next = pos + 12 + static_cast<std::size_t>(len); // len + type + data + crc
        if (next <= pos)
            return false;
        pos = next;
    }
    return false;
}

} // namespace

bool bytes_may_be_animated(std::span<const std::uint8_t> bytes)
{
    if (has_prefix(bytes, "GIF8"))
        return true;
    if (has_prefix(bytes, "RIFF") && has_prefix(bytes, "WEBP", 8) && has_prefix(bytes, "VP8X", 12))
        return bytes.size() > 20 && (bytes[20] & 0x02) != 0; // animation flag
    if (bytes.size() > 8 && std::memcmp(bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0)
        return png_has_actl_before_idat(bytes);
    return false;
}

} // namespace tk

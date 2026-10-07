#pragma once

#include <cstdint>
#include <span>

namespace tk
{

// Cheap header sniff (no decode): true when `bytes` is a GIF (even a
// single-frame one — only the decoder can tell), a WebP whose VP8X header
// has the animation flag set, or an APNG (an `acTL` chunk ahead of the first
// `IDAT`). False for anything else, including truncated input.
bool bytes_may_be_animated(std::span<const std::uint8_t> bytes);

} // namespace tk

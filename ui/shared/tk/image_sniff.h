#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace tk
{

// Cheap header sniff (no decode): true when `bytes` is a GIF (even a
// single-frame one — only the decoder can tell), a WebP whose VP8X header
// has the animation flag set, or an APNG (an `acTL` chunk ahead of the first
// `IDAT`). False for anything else, including truncated input.
bool bytes_may_be_animated(std::span<const std::uint8_t> bytes);

struct ImageDims
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// Width/height read from the header of a PNG, GIF, JPEG or WebP (VP8, VP8L,
// or the VP8X canvas) without decoding. nullopt for any other format or for
// input truncated before the size fields.
std::optional<ImageDims> probe_image_dimensions(std::span<const std::uint8_t> bytes);

// Pixel budget for decoding untrusted bytes. Stills: 100 MP and 32768 px per
// side (~400 MB RGBA). JPEG stills get 268 MP and 65535 px per side because
// every shell path decodes JPEG with DCT-domain scaling, so memory is bounded
// by the output size, and real 102-200 MP camera files must still open.
// Animations keep every frame plus compositing buffers, so they get 16 MP
// (4096x4096).
inline constexpr std::uint64_t kMaxStillDecodePixels = 100'000'000;
inline constexpr std::uint32_t kMaxDecodeSide = 32768;
inline constexpr std::uint64_t kMaxJpegStillDecodePixels = 1ull << 28; // 268 MP: JPEG decodes scaled in every shell path
inline constexpr std::uint32_t kMaxJpegDecodeSide = 65535;
inline constexpr std::uint64_t kMaxAnimatedDecodePixels = 4096ull * 4096ull;

// False only when the header is recognised and over budget. Unrecognised
// formats pass; the platform decoder decides.
bool decode_size_allowed(std::span<const std::uint8_t> bytes, bool animated);

} // namespace tk

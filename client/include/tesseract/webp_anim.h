#pragma once
#include <cstdint>
#include <memory>
#include <span>

namespace tesseract
{

/// True iff `bytes` parse as a (still or animated) WebP bitstream.
bool is_webp_data(std::span<const std::uint8_t> bytes);

/// Forward-only animated-WebP decoder (libwebp's WebPAnimDecoder, Rust-owned).
/// Keeps a persistent compositing canvas across sequential next_frame() calls,
/// which the macOS canvas relies on (ImageIO's per-frame cost grows with the
/// frame index). Owns a copy of the compressed bytes.
class WebpAnimDecoder
{
public:
    explicit WebpAnimDecoder(std::span<const std::uint8_t> bytes);
    ~WebpAnimDecoder();
    WebpAnimDecoder(WebpAnimDecoder&&) noexcept;
    WebpAnimDecoder& operator=(WebpAnimDecoder&&) noexcept;

    bool valid() const;
    std::uint32_t canvas_width() const;
    std::uint32_t canvas_height() const;
    std::uint32_t frame_count() const;
    bool has_more_frames() const;

    /// Decode the next frame. Returns premultiplied BGRA,
    /// `canvas_width() * canvas_height() * 4` bytes, valid until the next
    /// next_frame()/reset()/destruction; nullptr when exhausted or on failure.
    /// `timestamp_ms` is cumulative ms since the animation start (not this
    /// frame's own duration).
    const std::uint8_t* next_frame(int& timestamp_ms);

    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tesseract

#pragma once
#include <cstdint>
#include <memory>
#include <span>

namespace tesseract
{

/// True iff `bytes` start with a JPEG XL signature (bare codestream or
/// ISOBMFF container). Header sniff only; says nothing about validity.
bool is_jxl_data(std::span<const std::uint8_t> bytes);

/// Header-only: true when the JXL header declares an animation. False for
/// non-JXL or unparseable input. Never renders a frame.
bool is_jxl_animated(std::span<const std::uint8_t> bytes);

/// Sequential JPEG XL decoder (Rust `jxl-oxide`). A still is a single frame.
/// Output is straight (non-premultiplied) sRGB RGBA8 with the header
/// orientation applied, downscaled to fit `max_w x max_h` (never upscaled).
/// Over-budget or malformed input yields valid() == false. Owns no copy of
/// `bytes` beyond what the decoder needs; safe to call from a worker thread
/// (one thread at a time).
class JxlDecoder
{
public:
    JxlDecoder(std::span<const std::uint8_t> bytes, std::uint32_t max_w, std::uint32_t max_h);
    ~JxlDecoder();
    JxlDecoder(JxlDecoder&&) noexcept;
    JxlDecoder& operator=(JxlDecoder&&) noexcept;

    bool valid() const;
    /// Output size (post-orientation, post-downscale).
    std::uint32_t width() const;
    std::uint32_t height() const;
    /// Keyframes, capped at the shared animation frame limit.
    std::uint32_t frame_count() const;
    bool has_more_frames() const;

    /// Decode the next frame. Returns RGBA8, `width() * height() * 4` bytes,
    /// valid until the next next_frame()/reset()/destruction; nullptr when
    /// exhausted or on failure. `delay_ms` is this frame's own duration.
    const std::uint8_t* next_frame(int& delay_ms);

    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tesseract

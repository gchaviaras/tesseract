#pragma once
#include <cstdint>
#include <span>
#include <string_view>

namespace tesseract
{

/// True when the SDK can confidently strip this image's metadata (EXIF/GPS,
/// XMP, comments) on upload: the format is recognised and parsed, or is known
/// to carry none (BMP, QOI, bare JPEG XL). False for formats with no stripper
/// and for recognised files that fail to parse. Content-based; `mime_type` is
/// only a fallback hint. Header scans only.
bool image_metadata_strippable(std::span<const std::uint8_t> bytes, std::string_view mime_type);

} // namespace tesseract

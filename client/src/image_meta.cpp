#include "tesseract/image_meta.h"
#include "tesseract_sdk_bridge_cxx/bridge.h"

namespace tesseract
{

bool image_metadata_strippable(std::span<const std::uint8_t> bytes, std::string_view mime_type)
{
    return tesseract_ffi::image_metadata_strippable(
        rust::Slice<const std::uint8_t>(bytes.data(), bytes.size()),
        rust::Str(mime_type.data(), mime_type.size()));
}

} // namespace tesseract

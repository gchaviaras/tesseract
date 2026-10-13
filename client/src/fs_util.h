#pragma once

#include <filesystem>
#include <string_view>

namespace tesseract
{

/// Replaces `p` with `content` atomically: the bytes go to a private
/// `<p>.tmp` (mode 0600, created fresh), are fsync'd, then renamed over `p`.
/// A crash or power loss leaves either the old file or the new one, never a
/// truncated mix. Creates missing parent directories. Returns false on any
/// failure, leaving the previous file in place.
bool atomic_write_file(const std::filesystem::path& p, std::string_view content);

} // namespace tesseract

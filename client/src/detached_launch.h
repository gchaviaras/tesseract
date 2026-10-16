#pragma once

#include <string>

namespace tesseract::detail
{

// Starts `opener` with the single argument `arg` and returns without waiting for
// the opener itself. Returns false only when the launch could not be dispatched.
// POSIX only: callers on Windows use ShellExecute.
bool launch_detached(const char* opener, const std::string& arg);

} // namespace tesseract::detail

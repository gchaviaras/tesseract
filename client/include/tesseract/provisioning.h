#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tesseract
{

// Deployment-provisioned defaults, written by an installer or an admin rather
// than by the user. Sources per platform:
//   Windows: registry value "DefaultHomeserver" under HKCU\Software\Tesseract-Matrix,
//            then HKLM\Software\Tesseract-Matrix (the NSIS installer's
//            /DEFAULT_HOMESERVER=<host> switch writes the HKLM one).
//   macOS:   the "DefaultHomeserver" preference (managed/MDM prefs honoured).
//   Linux:   "default_homeserver" in /etc/tesseract/provisioning.json.

// Homeserver the login view should use instead of "matrix.org", or nullopt when
// none is provisioned. Evaluated once per process.
std::optional<std::string> provisioned_default_homeserver();

// Trims whitespace and trailing '/'; empty result -> nullopt.
std::optional<std::string> normalize_homeserver(std::string_view raw);

// Extracts "default_homeserver" from provisioning.json text (normalized).
// Invalid JSON or a missing/non-string key -> nullopt.
std::optional<std::string> parse_provisioning_json(std::string_view json_text);

namespace detail
{
// Per-platform raw read (provisioning_<platform>.cpp); unnormalized.
std::optional<std::string> read_provisioned_homeserver_raw();
} // namespace detail

} // namespace tesseract

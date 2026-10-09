#pragma once

// Pure helpers lifted out of ShellBase.cpp (no `this`, no shell state). Where
// the shells or tests call one through ShellBase (clamp_to_screens_,
// compute_tray_unread, account_has_unread, find_existing_dm), ShellBase keeps
// a one-line forwarding static.

#include <tesseract/client.h>
#include <tesseract/settings.h>
#include <tesseract/types.h>
#include "app/PresenceTracker.h"
#include "tk/canvas.h"
#include "tk/theme.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tesseract::shell_helpers
{

// Re-centres saved window geometry on the first screen when its title-bar
// strip is off every screen work area; returns it unchanged otherwise.
Settings::WindowGeometry clamp_to_screens(const Settings::WindowGeometry& saved,
                                          int default_w, int default_h,
                                          const std::vector<tk::Rect>& screens);

// (any unread, any highlight) across every account's rooms.
std::pair<bool, bool> compute_tray_unread(
    const std::unordered_map<std::string, std::vector<RoomInfo>>& by_account);
bool account_has_unread(const std::vector<RoomInfo>& rooms);

// Room id of the existing DM with user_id, or empty.
std::string find_existing_dm(const std::vector<RoomInfo>& rooms,
                             const std::string& user_id);

// "A is typing…" / "A and B are typing…" / "A, B and N others are typing…".
std::string format_typing_text(const std::vector<std::string>& names);

tesseract::Settings::MediaPreviews
mode_to_settings(tesseract::MediaPreviewConfig::Mode m);
tesseract::PresenceState to_client_presence(PresenceTracker::State s);
tk::AccentTheme to_tk_accent(tesseract::Settings::ThemeAccent accent);

// Write a secret to `path` owner-only (0600 on POSIX); sets `error` on failure.
bool write_private_text_file(const std::string& path, const std::string& text,
                             std::string& error);

} // namespace tesseract::shell_helpers

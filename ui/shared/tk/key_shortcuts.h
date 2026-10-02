#pragma once

#include "tk/widget.h"

#include <cctype>

namespace tk
{

// True when `event` carries the platform's primary shortcut modifier —
// Ctrl on Windows/Linux, Cmd (reported as meta) on macOS — without Alt.
// Shift is left to the caller: some shortcuts use it as a variant.
inline bool primary_shortcut(const KeyEvent& event)
{
    return (event.ctrl || event.meta) && !event.alt;
}

// True when `event` is a single-character key matching `ch`,
// case-insensitively (Caps Lock / Shift report the uppercase letter).
inline bool shortcut_char(const KeyEvent& event, char ch)
{
    if (event.key != Key::Character || event.text.size() != 1)
        return false;
    return static_cast<char>(std::tolower(
               static_cast<unsigned char>(event.text.front()))) == ch;
}

} // namespace tk

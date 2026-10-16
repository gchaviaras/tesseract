#pragma once
#include "views/ComposeBar.h"

#include <string>
#include <vector>

namespace tesseract
{

/// What the composer held when an attachment send started: the attachments as
/// the user attached them (not the re-encoded copies that go out) and the
/// caption. Kept until the upload's outcome is known, so a failure can put it
/// back in the composer instead of losing it.
struct UploadDraft
{
    std::string user_id; // the account that sent it
    std::string room_id;
    std::string caption;
    std::vector<views::ComposeBar::PendingAttachment> items;
};

} // namespace tesseract

#pragma once

// Settings panel section: language selection via a combobox.

#include "SettingBinder.h"
#include "SettingsPage.h"
#include "tk/combobox.h"

#include <functional>
#include <string>

namespace tesseract::views
{

class LanguageSection : public SettingsPage
{
public:
    explicit LanguageSection(SettingBinder& binder);
    ~LanguageSection() override = default;

    // Show the "Restart now" button — set while the saved language differs
    // from the one this process started with.
    void set_restart_pending(bool pending);

    // Fires when the user presses "Restart now".
    std::function<void()> on_restart_requested;

private:
    tk::ComboBox* combo_ = nullptr;
    tk::Widget* restart_row_ = nullptr;
};

} // namespace tesseract::views

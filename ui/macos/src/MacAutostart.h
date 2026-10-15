#pragma once
#include <tesseract/autostart.h>

namespace mac
{

// IAutostart impl backed by SMAppService.mainApp (macOS 13+, which is also
// the deployment target).
class MacAutostart final : public tesseract::IAutostart
{
public:
    bool is_enabled() const override;
    bool set_enabled(bool enabled) override;
};

} // namespace mac

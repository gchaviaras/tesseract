#include "tesseract/provisioning.h"

#include <CoreFoundation/CoreFoundation.h>

namespace tesseract::detail
{

std::optional<std::string> read_provisioned_homeserver_raw()
{
    // CFPreferences consults managed (MDM) preferences before user ones.
    CFPropertyListRef value = CFPreferencesCopyAppValue(CFSTR("DefaultHomeserver"),
                                                        kCFPreferencesCurrentApplication);
    if (!value)
        return std::nullopt;

    std::optional<std::string> result;
    if (CFGetTypeID(value) == CFStringGetTypeID())
    {
        auto str = static_cast<CFStringRef>(value);
        CFIndex max = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str),
                                                        kCFStringEncodingUTF8) + 1;
        std::string buf(static_cast<size_t>(max), '\0');
        if (CFStringGetCString(str, buf.data(), max, kCFStringEncodingUTF8))
        {
            buf.resize(std::char_traits<char>::length(buf.c_str()));
            result = std::move(buf);
        }
    }
    CFRelease(value);
    return result;
}

} // namespace tesseract::detail

#include "tesseract/provisioning.h"

#include <fstream>
#include <iterator>

namespace tesseract::detail
{

std::optional<std::string> read_provisioned_homeserver_raw()
{
    std::ifstream in("/etc/tesseract/provisioning.json");
    if (!in)
        return std::nullopt;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse_provisioning_json(text);
}

} // namespace tesseract::detail

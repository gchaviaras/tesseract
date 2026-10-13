#include "tesseract/provisioning.h"

#include <nlohmann/json.hpp>

namespace tesseract
{

std::optional<std::string> normalize_homeserver(std::string_view raw)
{
    auto is_junk = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!raw.empty() && is_junk(raw.front()))
        raw.remove_prefix(1);
    while (!raw.empty() && (is_junk(raw.back()) || raw.back() == '/'))
        raw.remove_suffix(1);
    if (raw.empty())
        return std::nullopt;
    return std::string(raw);
}

std::optional<std::string> parse_provisioning_json(std::string_view json_text)
{
    auto j = nlohmann::json::parse(json_text.begin(), json_text.end(), nullptr, false);
    if (!j.is_object())
        return std::nullopt;
    auto it = j.find("default_homeserver");
    if (it == j.end() || !it->is_string())
        return std::nullopt;
    return normalize_homeserver(it->get<std::string>());
}

std::optional<std::string> provisioned_default_homeserver()
{
    static const std::optional<std::string> value = [] {
        auto raw = detail::read_provisioned_homeserver_raw();
        return raw ? normalize_homeserver(*raw) : std::nullopt;
    }();
    return value;
}

} // namespace tesseract

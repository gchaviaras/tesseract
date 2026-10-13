#include <catch2/catch_test_macros.hpp>

#include <tesseract/provisioning.h>

using namespace tesseract;

TEST_CASE("normalize_homeserver trims whitespace and trailing slashes", "[provisioning]")
{
    CHECK(normalize_homeserver("example.org") == "example.org");
    CHECK(normalize_homeserver("  example.org \r\n") == "example.org");
    CHECK(normalize_homeserver("https://example.org/") == "https://example.org");
    CHECK(normalize_homeserver("example.org:8448//") == "example.org:8448");
}

TEST_CASE("normalize_homeserver rejects empty input", "[provisioning]")
{
    CHECK_FALSE(normalize_homeserver("").has_value());
    CHECK_FALSE(normalize_homeserver("  \t ").has_value());
    CHECK_FALSE(normalize_homeserver("/").has_value());
}

TEST_CASE("parse_provisioning_json reads default_homeserver", "[provisioning]")
{
    CHECK(parse_provisioning_json(R"({"default_homeserver": " example.org/ "})") == "example.org");
    CHECK_FALSE(parse_provisioning_json("{}").has_value());
    CHECK_FALSE(parse_provisioning_json(R"({"default_homeserver": 5})").has_value());
    CHECK_FALSE(parse_provisioning_json(R"({"default_homeserver": ""})").has_value());
    CHECK_FALSE(parse_provisioning_json("not json").has_value());
    CHECK_FALSE(parse_provisioning_json("[1,2]").has_value());
}

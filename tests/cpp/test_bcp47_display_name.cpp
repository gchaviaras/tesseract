#include <catch2/catch_test_macros.hpp>

#include "views/settings/bcp47_languages.h"

using tesseract::views::bcp47_display_name;

TEST_CASE("bcp47_display_name names the language", "[bcp47]")
{
    CHECK(bcp47_display_name("fr") == "French");
    CHECK(bcp47_display_name("FR") == "French");
}

TEST_CASE("bcp47_display_name appends region and script", "[bcp47]")
{
    CHECK(bcp47_display_name("en-US") == "English (United States)");
    CHECK(bcp47_display_name("en_gb") == "English (United Kingdom)");
    CHECK(bcp47_display_name("es-419") == "Spanish (Latin America)");
    CHECK(bcp47_display_name("zh-Hans-CN") == "Chinese (Simplified, China)");
}

TEST_CASE("bcp47_display_name degrades gracefully on unknown input", "[bcp47]")
{
    // Unknown language: shown as written.
    CHECK(bcp47_display_name("xx-YY") == "xx-YY");
    CHECK(bcp47_display_name("").empty());
    // Unknown region is uppercased; unrecognised subtags are dropped.
    CHECK(bcp47_display_name("en-zz") == "English (ZZ)");
    CHECK(bcp47_display_name("en-x-private") == "English");
    // Trailing/leading separators don't crash.
    CHECK(bcp47_display_name("en-") == "English");
}

#include <catch2/catch_test_macros.hpp>

#include "app/Launch.h"

using tesseract::Settings;

TEST_CASE("apply_proxy_live moves the launch baseline so no restart is pending",
          "[launch][proxy]")
{
    const std::string url = "http://proxy.example.com:3128";
    REQUIRE_FALSE(tesseract::proxy_changed_since_launch(Settings::ProxyMode::System, ""));

    tesseract::apply_proxy_live(Settings::ProxyMode::Manual, url);
    CHECK_FALSE(tesseract::proxy_changed_since_launch(Settings::ProxyMode::Manual, url));
    CHECK(tesseract::proxy_changed_since_launch(Settings::ProxyMode::None, ""));

    // An invalid Manual URL fails closed in the SDK, so the baseline stays
    // Manual: switching away from it is a real change.
    tesseract::apply_proxy_live(Settings::ProxyMode::Manual, "garbage");
    CHECK_FALSE(tesseract::proxy_changed_since_launch(Settings::ProxyMode::Manual, "garbage"));
    CHECK(tesseract::proxy_changed_since_launch(Settings::ProxyMode::System, ""));

    tesseract::apply_proxy_live(Settings::ProxyMode::System, "");
    CHECK_FALSE(tesseract::proxy_changed_since_launch(Settings::ProxyMode::System, ""));
}

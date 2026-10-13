#include <catch2/catch_test_macros.hpp>

#include "app/Launch.h"

using tesseract::Settings;

TEST_CASE("apply_proxy_live keeps the restart pending: call signalling still uses the launch proxy",
          "[launch][proxy]")
{
    const std::string url = "http://proxy.example.com:3128";
    REQUIRE_FALSE(tesseract::proxy_changed_since_launch(Settings::ProxyMode::System, ""));

    // Applying a Manual proxy live moves HTTP, not the launch baseline.
    tesseract::apply_proxy_live(Settings::ProxyMode::Manual, url);
    CHECK(tesseract::proxy_changed_since_launch(Settings::ProxyMode::Manual, url));
    CHECK_FALSE(tesseract::proxy_changed_since_launch(Settings::ProxyMode::System, ""));

    // A bad Manual URL fails closed in the SDK and is still a pending change.
    tesseract::apply_proxy_live(Settings::ProxyMode::Manual, "garbage");
    CHECK(tesseract::proxy_changed_since_launch(Settings::ProxyMode::Manual, "garbage"));

    // Back to the launch setting: nothing pending, and the SDK is restored.
    tesseract::apply_proxy_live(Settings::ProxyMode::System, "");
    CHECK_FALSE(tesseract::proxy_changed_since_launch(Settings::ProxyMode::System, ""));
}

#include <catch2/catch_test_macros.hpp>
#include <tesseract/client.h>

using tesseract::Client;

TEST_CASE("is_launchable_url: http and https are allowed")
{
    CHECK(Client::is_launchable_url("https://matrix.org"));
    CHECK(Client::is_launchable_url("http://example.org/path"));
}

TEST_CASE("is_launchable_url: scheme match is case-insensitive")
{
    CHECK(Client::is_launchable_url("HTTPS://matrix.org"));
    CHECK(Client::is_launchable_url("Http://example.org"));
}

TEST_CASE("is_launchable_url: real login and account URLs stay launchable")
{
    CHECK(Client::is_launchable_url(
        "https://account.example.org/oauth2/auth?response_type=code&client_id=01J"
        "&redirect_uri=http%3A%2F%2F127.0.0.1%3A43123%2F&scope=urn%3Amatrix"
        "&state=abc&code_challenge=xyz&code_challenge_method=S256#frag"));
    CHECK(Client::is_launchable_url("https://bücher.example/konto"));
}

TEST_CASE("is_launchable_url: non-web schemes are refused")
{
    CHECK_FALSE(Client::is_launchable_url("file:///etc/passwd"));
    CHECK_FALSE(Client::is_launchable_url("search-ms:query=x"));
    CHECK_FALSE(Client::is_launchable_url("ms-msdt:/id PCWDiagnostic"));
    CHECK_FALSE(Client::is_launchable_url("javascript:alert(1)"));
    CHECK_FALSE(Client::is_launchable_url("smb://host/share"));
    CHECK_FALSE(Client::is_launchable_url("matrix:r/room:example.org"));
    CHECK_FALSE(Client::is_launchable_url("httpx://example.org"));
}

TEST_CASE("is_launchable_url: option-like and malformed input is refused")
{
    CHECK_FALSE(Client::is_launchable_url(""));
    CHECK_FALSE(Client::is_launchable_url("-aCalculator"));
    CHECK_FALSE(Client::is_launchable_url("/Applications/Calculator.app"));
    CHECK_FALSE(Client::is_launchable_url(" https://example.org"));
    CHECK_FALSE(Client::is_launchable_url("https://exa mple.org"));
    CHECK_FALSE(Client::is_launchable_url("https://example.org\n--help"));
    CHECK_FALSE(Client::is_launchable_url(std::string("https://a\x7f", 10)));
    CHECK_FALSE(Client::is_launchable_url("https://example.org/\"--x"));
}

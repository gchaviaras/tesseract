#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "views/call_tile_layout.h"

using Catch::Approx;
using tesseract::views::best_tile_grid;
using tesseract::views::layout_tiles;

TEST_CASE("call tiles: wide strip lays out as one row", "[call_tile_layout]")
{
    const auto g = best_tile_grid(4, 1000.0f, 180.0f);
    CHECK(g.cols == 4);
    CHECK(g.rows == 1);
}

TEST_CASE("call tiles: tall narrow area lays out as one column", "[call_tile_layout]")
{
    const auto g = best_tile_grid(3, 300.0f, 1000.0f);
    CHECK(g.cols == 1);
    CHECK(g.rows == 3);
}

TEST_CASE("call tiles: roughly 4:3 area uses a balanced grid", "[call_tile_layout]")
{
    const auto g = best_tile_grid(4, 800.0f, 600.0f);
    CHECK(g.cols == 2);
    CHECK(g.rows == 2);
}

TEST_CASE("call tiles: single participant fills the area", "[call_tile_layout]")
{
    const auto g = best_tile_grid(1, 500.0f, 500.0f);
    CHECK(g.cols == 1);
    CHECK(g.rows == 1);

    const auto r = layout_tiles(1, {10.0f, 20.0f, 500.0f, 400.0f});
    REQUIRE(r.size() == 1);
    CHECK(r[0].x == Approx(10.0f));
    CHECK(r[0].y == Approx(20.0f));
    CHECK(r[0].w == Approx(500.0f));
    CHECK(r[0].h == Approx(400.0f));
}

TEST_CASE("call tiles: partial last row is centred", "[call_tile_layout]")
{
    // 3 tiles in a 4:3-ish box → 2×2 with one tile alone on the second row.
    const auto r = layout_tiles(3, {0.0f, 0.0f, 800.0f, 600.0f});
    REQUIRE(r.size() == 3);
    CHECK(r[0].x == Approx(0.0f));
    CHECK(r[1].x == Approx(400.0f));
    CHECK(r[2].y == Approx(300.0f));
    CHECK(r[2].x == Approx(200.0f));
    CHECK(r[2].w == Approx(400.0f));
}

TEST_CASE("call tiles: degenerate input doesn't crash", "[call_tile_layout]")
{
    CHECK(layout_tiles(0, {0.0f, 0.0f, 100.0f, 100.0f}).empty());
    const auto r = layout_tiles(3, {0.0f, 0.0f, -5.0f, 0.0f});
    REQUIRE(r.size() == 3);
    for (const auto& c : r)
    {
        CHECK(c.w >= 0.0f);
        CHECK(c.h >= 0.0f);
    }
}

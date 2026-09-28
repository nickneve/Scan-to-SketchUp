#include <doctest/doctest.h>

#include "mesh.hpp"
#include "polygon.hpp"

using namespace s2s;

TEST_CASE("signed area is positive for counter-clockwise loops") {
    Loop2 sq{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    CHECK(signed_area(sq) == doctest::Approx(100));
    Loop2 rev(sq.rbegin(), sq.rend());
    CHECK(signed_area(rev) == doctest::Approx(-100));
}

TEST_CASE("clipping keeps the requested half") {
    Loop2 sq{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    Loop2 left = clip_half_plane(sq, {1, 0}, 4);
    CHECK(signed_area(left) == doctest::Approx(40));
    CHECK(left.size() == 4);
}

TEST_CASE("rectangle minus a window leaves one face with a hole") {
    auto regions = rect_minus_rects({0, 0, 100, 50}, {{20, 10, 40, 30}});
    REQUIRE(regions.size() == 1);
    CHECK(regions[0].outer.size() == 4);
    REQUIRE(regions[0].holes.size() == 1);
    CHECK(regions[0].holes[0].size() == 4);
    CHECK(region_area(regions[0]) == doctest::Approx(5000 - 400));
}

TEST_CASE("rectangle minus a door leaves one notched face") {
    auto regions = rect_minus_rects({0, 0, 100, 50}, {{20, 0, 40, 30}});
    REQUIRE(regions.size() == 1);
    CHECK(regions[0].holes.empty());
    CHECK(regions[0].outer.size() == 8);
    CHECK(region_area(regions[0]) == doctest::Approx(5000 - 600));
}

TEST_CASE("a full-height cut splits the face in two") {
    auto regions = rect_minus_rects({0, 0, 100, 50}, {{20, 0, 40, 50}});
    CHECK(regions.size() == 2);
}

TEST_CASE("door and windows together") {
    auto regions = rect_minus_rects({0, 0, 300, 100}, {{20, 0, 60, 80}, {100, 30, 140, 70}, {200, 30, 240, 70}});
    REQUIRE(regions.size() == 1);
    CHECK(regions[0].holes.size() == 2);
    CHECK(region_area(regions[0]) == doctest::Approx(30000 - 3200 - 1600 - 1600));
}

TEST_CASE("triangulation covers the region exactly") {
    for (const auto& cuts : std::vector<std::vector<Rect>>{
             {}, {{20, 10, 40, 30}}, {{20, 0, 40, 30}}, {{10, 10, 20, 20}, {50, 10, 60, 40}, {70, 0, 90, 45}}}) {
        for (const Region& r : rect_minus_rects({0, 0, 100, 50}, cuts)) {
            double area = 0;
            for (const auto& t : triangulate(r)) area += signed_area({t[0], t[1], t[2]});
            CHECK(area == doctest::Approx(region_area(r)));
        }
    }
}

TEST_CASE("a box is a closed solid with the right volume") {
    SolidReport r = check_solid(box({0, 0, 0}, {2, 3, 4}));
    CHECK(r.closed);
    CHECK(r.volume == doctest::Approx(24));
}

TEST_CASE("an extruded ring (window frame) is a closed solid") {
    Region ring{{{0, 0}, {10, 0}, {10, 10}, {0, 10}}, {{{2, 2}, {2, 8}, {8, 8}, {8, 2}}}};
    SolidReport r = check_solid(extrude(ring, {0, 5, 0}, {1, 0, 0}, {0, 0, 1}, 5));
    CHECK(r.closed);
    CHECK(r.volume == doctest::Approx((100 - 36) * 5));
}

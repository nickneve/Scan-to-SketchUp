#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "helpers.hpp"
#include "mesh.hpp"
#include "rooms.hpp"
#include "walls.hpp"

using namespace s2s;

namespace {

struct Layout {
    std::map<std::string, WallFootprint> walls;
    std::vector<RoomShape> rooms;
};

Layout layout(const Plan& p) {
    const Plan plan = normalize_origin(p);
    Layout l{compute_footprints(plan.levels[0]), {}};
    for (const Room& r : plan.levels[0].rooms) l.rooms.push_back(room_shape(plan.levels[0], r));
    return l;
}

// Every sample point inside the house outline must be covered by exactly one wall or room,
// so walls and floors meet with no gaps and no overlaps.
void check_tiling(const Layout& l, const Loop2& outline) {
    double x0 = 1e18, x1 = -1e18, y0 = 1e18, y1 = -1e18;
    for (Vec2 p : outline) {
        x0 = std::min(x0, p.x);
        x1 = std::max(x1, p.x);
        y0 = std::min(y0, p.y);
        y1 = std::max(y1, p.y);
    }
    int bad = 0, samples = 0;
    for (double x = x0 + 11.3; x < x1; x += 37)
        for (double y = y0 + 7.9; y < y1; y += 37) {
            Vec2 p{x, y};
            if (!point_in_polygon(p, outline)) continue;
            int hits = 0;
            for (const auto& [id, fp] : l.walls) hits += point_in_polygon(p, fp.polygon);
            for (const RoomShape& r : l.rooms) hits += point_in_polygon(p, r.interior);
            ++samples;
            if (hits != 1 && ++bad <= 5) MESSAGE("point (" << x << ", " << y << ") covered " << hits << " times");
        }
    CHECK(samples > 1000);
    CHECK(bad == 0);

    double area = 0;
    for (const auto& [id, fp] : l.walls) area += signed_area(fp.polygon);
    for (const RoomShape& r : l.rooms) area += r.area();
    CHECK(area == doctest::Approx(signed_area(outline)).epsilon(1e-9));
}

}  // namespace

TEST_CASE("walls and rooms tile the house with no gaps or overlaps") {
    SUBCASE("rectangular room") {
        check_tiling(layout(test::fixture("01-rectangular-room")), {{0, 0}, {3988, 0}, {3988, 3378}, {0, 3378}});
    }
    SUBCASE("L-shaped room") {
        check_tiling(layout(test::fixture("02-l-shaped-room")),
                     {{0, 0}, {5207, 0}, {5207, 2768}, {2768, 2768}, {2768, 4597}, {0, 4597}});
    }
    SUBCASE("three rooms and a hallway") {
        check_tiling(layout(test::fixture("03-three-rooms-hallway")), {{0, 0}, {9474, 0}, {9474, 6426}, {0, 6426}});
    }
}

TEST_CASE("corners are mitred and interior walls stop at the face they meet") {
    const Layout l = layout(test::fixture("03-three-rooms-hallway"));
    // Front-left corner: two exterior walls mitred along the diagonal.
    const Loop2& e1 = l.walls.at("E1").polygon;
    CHECK(std::any_of(e1.begin(), e1.end(), [](Vec2 p) { return distance(p, {0, 0}) < 1e-6; }));
    CHECK(std::any_of(e1.begin(), e1.end(), [](Vec2 p) { return distance(p, {165, 165}) < 1e-6; }));
    // I1 tees into the front wall: it starts at the front wall's inside face (y = 165).
    double minY = 1e18;
    for (Vec2 p : l.walls.at("I1").polygon) minY = std::min(minY, p.y);
    CHECK(minY == doctest::Approx(165));
    // The front wall runs straight through the tee: E1 and E2 meet on a square cut at x = 4500.
    const Loop2& e2 = l.walls.at("E2").polygon;
    CHECK(std::all_of(e2.begin(), e2.end(), [](Vec2 p) {
        return std::abs(p.x - 4500) < 1e-6 || std::abs(p.x - 5700) < 1e-6;
    }));
    // Separators have no footprint.
    CHECK(l.walls.count("S1") == 0);
}

TEST_CASE("every wall is a closed solid with the openings removed") {
    for (const char* name : {"01-rectangular-room", "02-l-shaped-room", "03-three-rooms-hallway"}) {
        CAPTURE(name);
        const Plan plan = normalize_origin(test::fixture(name));
        const Level& level = plan.levels[0];
        const auto fps = compute_footprints(level);
        const auto heights = wall_heights(level);
        for (const Wall& w : level.walls) {
            if (w.separator) continue;
            CAPTURE(w.id);
            const auto cuts = wall_cuts(level, w.id);
            const auto faces = build_wall_solid(w, fps.at(w.id), level.elevation, heights.at(w.id), cuts);
            const SolidReport r = check_solid(faces);
            for (const auto& p : r.problems) MESSAGE(p);
            CHECK(r.closed);
            double expected = signed_area(fps.at(w.id).polygon) * heights.at(w.id);
            for (const WallCut& c : cuts) expected -= (c.u1 - c.u0) * (c.z1 - c.z0) * w.thickness;
            CHECK(r.volume == doctest::Approx(expected).epsilon(1e-9));
        }
    }
}

TEST_CASE("walls take the tallest ceiling of the rooms they bound") {
    const Plan plan = test::fixture("02-l-shaped-room");
    const auto h = wall_heights(plan.levels[0]);
    CHECK(h.at("W1") == doctest::Approx(2743));  // Great Room has a 9 ft ceiling
}

TEST_CASE("openings that run into a corner are rejected") {
    Plan plan = test::fixture("01-rectangular-room");
    test::opening(plan, "D1").offsetFromStart = 20;  // inside face starts 82.5 mm along the centerline
    CHECK_THROWS_AS(build_scene(plan), PlanError);
}

TEST_CASE("touching openings are rejected") {
    Plan plan = test::fixture("01-rectangular-room");
    Opening second = test::opening(plan, "D1");
    second.id = "D2";
    second.offsetFromStart += second.width;
    plan.levels[0].openings.push_back(second);
    CHECK_THROWS_AS(build_scene(plan), PlanError);
}

TEST_CASE("a full-height opening splits the wall into two pieces that are still solid") {
    Plan plan = test::fixture("01-rectangular-room");
    Opening& d = test::opening(plan, "D1");
    d.height = 2438;  // full height: the wall above the door disappears too
    const Scene scene = build_scene(plan);
    const Group* walls = test::level_group(scene).find("Walls");
    REQUIRE(walls);
    const auto r = check_solid(walls->groups[0].faces);
    // Two separate pieces are still a closed (if disconnected) shell.
    CHECK(r.closed);
}

TEST_CASE("a wall with a door and several windows stays solid") {
    Plan plan = test::fixture("01-rectangular-room");
    Opening w = test::opening(plan, "N1");
    auto add = [&](const std::string& id, const std::string& wallId, double offset, double width, double sill,
                   double height) {
        Opening o = w;
        o.id = id;
        o.wallId = wallId;
        o.offsetFromStart = offset;
        o.width = width;
        o.sillHeight = sill;
        o.height = height;
        plan.levels[0].openings.push_back(o);
    };
    add("N2", "W3", 400, 600, 900, 900);
    add("N3", "W3", 2600, 700, 1200, 600);
    add("N4", "W1", 2300, 900, 900, 1200);
    add("N5", "W1", 3300, 300, 300, 300);
    const Scene scene = build_scene(plan);
    for (const Group& g : test::level_group(scene).find("Walls")->groups) {
        const SolidReport r = check_solid(g.faces);
        for (const auto& p : r.problems) MESSAGE(p);
        CHECK_MESSAGE(r.closed, g.name);
    }
    // W3: 3 windows. Volume = footprint * height - windows.
    const Plan normalized = normalize_origin(plan);
    const Level& level = normalized.levels[0];
    const auto fps = compute_footprints(level);
    const Wall& w3 = *level.find_wall("W3");
    const auto cuts = wall_cuts(level, "W3");
    CHECK(cuts.size() == 3);
    const auto faces = build_wall_solid(w3, fps.at("W3"), 0, 2438, cuts);
    double expected = signed_area(fps.at("W3").polygon) * 2438;
    for (const WallCut& c : cuts) expected -= (c.u1 - c.u0) * (c.z1 - c.z0) * w3.thickness;
    CHECK(check_solid(faces).volume == doctest::Approx(expected).epsilon(1e-9));
}

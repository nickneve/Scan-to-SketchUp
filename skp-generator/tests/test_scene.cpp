// Checks the SketchUp modeling conventions from docs/PROJECT_BRIEF.md against the built scene.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <set>

#include "flatten.hpp"
#include "helpers.hpp"
#include "mesh.hpp"

using namespace s2s;

namespace {

std::vector<std::string> child_names(const Group& g) {
    std::vector<std::string> out;
    for (const Group& c : g.groups) out.push_back(c.name);
    return out;
}

const char* kFixtures[] = {"01-rectangular-room", "02-l-shaped-room", "03-three-rooms-hallway"};

}  // namespace

TEST_CASE("hierarchy: House > Level > category groups, nothing loose at the top") {
    for (const char* name : kFixtures) {
        CAPTURE(name);
        const Scene scene = build_scene(test::fixture(name));
        REQUIRE(scene.groups.size() == 1);
        const Group& house = scene.groups[0];
        CHECK(house.name == "House");
        CHECK(house.faces.empty());
        REQUIRE(house.groups.size() == 1);
        const Group& level = house.groups[0];
        CHECK(level.name == "Level 1 - Main Floor");
        const auto names = child_names(level);
        const std::vector<std::string> order{"Walls", "Floors", "Ceilings", "Doors", "Windows", "Dimensions"};
        for (const auto& n : names) CHECK(std::find(order.begin(), order.end(), n) != order.end());
        CHECK(std::is_sorted(names.begin(), names.end(), [&](const std::string& a, const std::string& b) {
            return std::find(order.begin(), order.end(), a) < std::find(order.begin(), order.end(), b);
        }));
    }
}

TEST_CASE("tags: defaults, visibility, and tags only on groups and instances") {
    const Scene scene = build_scene(test::fixture("03-three-rooms-hallway"));
    for (const char* t : {"Walls", "Floors", "Ceilings", "Doors", "Windows", "Door Swings", "Dimensions"})
        CHECK_MESSAGE(scene.find_tag(t), t);
    CHECK_FALSE(scene.find_tag("Ceilings")->visible);
    CHECK(scene.find_tag("Walls")->visible);

    const Group& level = test::level_group(scene);
    auto expect_children_tagged = [&](const char* category, const char* tag) {
        const Group* g = level.find(category);
        REQUIRE(g);
        for (const Group& c : g->groups) CHECK(c.tag == tag);
        for (const Instance& i : g->instances) CHECK(i.tag == tag);
    };
    expect_children_tagged("Walls", "Walls");
    expect_children_tagged("Floors", "Floors");
    expect_children_tagged("Ceilings", "Ceilings");
    expect_children_tagged("Doors", "Doors");
    expect_children_tagged("Windows", "Windows");
    CHECK(level.find("Dimensions")->tag == "Dimensions");

    // Containers hold no raw geometry; geometry lives only in leaf groups.
    test::visit(scene.groups[0], [](const Group& g, int) {
        if (!g.groups.empty() || !g.instances.empty()) CHECK_MESSAGE(g.faces.empty(), g.name);
    });
}

TEST_CASE("walls: one solid group per wall, named by room and compass side") {
    SUBCASE("rectangular room, north angle 17.5") {
        const Scene scene = build_scene(test::fixture("01-rectangular-room"));
        const Group* walls = test::level_group(scene).find("Walls");
        REQUIRE(walls);
        CHECK(child_names(*walls) ==
              std::vector<std::string>{"Wall - Office South", "Wall - Office East", "Wall - Office North",
                                       "Wall - Office West"});
    }
    SUBCASE("shared walls list both rooms; duplicate names are numbered") {
        const Scene scene = build_scene(test::fixture("03-three-rooms-hallway"));
        const Group* walls = test::level_group(scene).find("Walls");
        REQUIRE(walls);
        CHECK(walls->groups.size() == 13);  // 14 walls minus the separator
        const auto names = child_names(*walls);
        CHECK(std::count(names.begin(), names.end(), "Wall - Bedroom / Hallway") == 1);
        CHECK(std::count(names.begin(), names.end(), "Wall - Bathroom / Bedroom") == 1);

        const Scene l = build_scene(test::fixture("02-l-shaped-room"));
        const auto lnames = child_names(*test::level_group(l).find("Walls"));
        CHECK(std::count(lnames.begin(), lnames.end(), "Wall - Great Room North 1") == 1);
        CHECK(std::count(lnames.begin(), lnames.end(), "Wall - Great Room North 2") == 1);
    }
    SUBCASE("every wall is solid") {
        for (const char* name : kFixtures) {
            const Scene scene = build_scene(test::fixture(name));
            for (const Group& w : test::level_group(scene).find("Walls")->groups) CHECK_MESSAGE(check_solid(w.faces).closed, w.name);
        }
    }
}

TEST_CASE("floors and ceilings: one per room, facing the right way") {
    const Scene scene = build_scene(test::fixture("02-l-shaped-room"));
    const Group& level = test::level_group(scene);
    const Group& floor = level.find("Floors")->groups.at(0);
    const Group& ceiling = level.find("Ceilings")->groups.at(0);
    CHECK(floor.name == "Floor - Great Room");
    CHECK(ceiling.name == "Ceiling - Great Room");
    CHECK(face_normal(floor.faces[0]).z == doctest::Approx(1));
    CHECK(face_normal(ceiling.faces[0]).z == doctest::Approx(-1));
    CHECK(ceiling.faces[0].loops[0][0].z == doctest::Approx(2743));  // room's own ceiling height
}

TEST_CASE("doors and windows: components named by size, shared by identical openings") {
    const Scene scene = build_scene(test::fixture("03-three-rooms-hallway"));
    std::set<std::string> defs;
    for (const auto& d : scene.definitions) defs.insert(d.name);
    CHECK(defs == std::set<std::string>{"Door 30x80", "Door 32x80", "Door 36x80", "Window 24x24", "Window 36x48",
                                        "Window 48x48"});
    for (const auto& d : scene.definitions)
        for (const Group& g : d.contents.groups)
            if (!g.faces.empty()) CHECK_MESSAGE(check_solid(g.faces).closed, (d.name + "/" + g.name));

    SUBCASE("same size in different wall thicknesses gets distinct names") {
        Plan plan = test::fixture("03-three-rooms-hallway");
        test::opening(plan, "D2").width = 914;  // now a 36x80 door in a 4.5in wall, like the 6.5in front door
        const Scene s = build_scene(plan);
        CHECK(s.find_definition("Door 36x80 (4.5in wall)"));
        CHECK(s.find_definition("Door 36x80 (6.5in wall)"));
    }
    SUBCASE("metric plans name components in millimeters") {
        Plan plan = test::fixture("01-rectangular-room");
        plan.displayUnits = DisplayUnits::Metric;
        CHECK(build_scene(plan).find_definition("Door 914x2032"));
    }
}

TEST_CASE("door swings open into the side the plan says") {
    const Scene scene = build_scene(test::fixture("03-three-rooms-hallway"));
    // D2 is on wall I2 (x = 5700, running +y) and opens to its right: into the bedroom (x > 5700).
    // D3 is on wall I3 and opens right: into the bathroom (x > 5700).
    for (const FlatPart& p : flatten(scene, true)) {
        if (p.tag != "Door Swings") continue;
        double maxX = -1e18, minX = 1e18;
        for (const auto& l : p.lines)
            for (Vec3 v : l) {
                maxX = std::max(maxX, v.x);
                minX = std::min(minX, v.x);
            }
        if (p.path.find("Door 32x80") != std::string::npos || p.path.find("Door 30x80") != std::string::npos) {
            CHECK(minX >= 5700 - 1);
            CHECK(maxX > 5757 + 500);
        }
        if (p.path.find("Door 36x80") != std::string::npos) {  // front door swings in, into the hallway
            for (const auto& l : p.lines)
                for (Vec3 v : l) CHECK(v.y >= 165 - 1);
        }
    }
}

TEST_CASE("dimensions: blueprint set") {
    const Scene scene = build_scene(test::fixture("01-rectangular-room"));
    const Group* g = test::level_group(scene).find("Dimensions");
    REQUIRE(g);
    // Room: 2 (opposite sides shown once). Outside: front and back get an opening string
    // plus overall (3 + 1 each); the sides get the overall only.
    CHECK(g->dimensions.size() == 12);
    bool overallFront = false, roomWidth = false;
    for (const Dimension& d : g->dimensions) {
        const double len = distance(d.start, d.end);
        if (std::abs(len - 3988) < 0.01 && d.offset.y < -1000) overallFront = true;
        if (std::abs(len - 3658) < 0.01 && d.offset.y > 0) roomWidth = true;
        CHECK(d.start.z == doctest::Approx(0));
    }
    CHECK(overallFront);
    CHECK(roomWidth);
}

TEST_CASE("origin moves to the front-left exterior corner") {
    Plan plan = test::fixture("01-rectangular-room");
    for (Wall& w : plan.levels[0].walls) {
        w.start = w.start + Vec2{1000, 500};
        w.end = w.end + Vec2{1000, 500};
    }
    const Scene scene = build_scene(plan);
    const Face& floor = test::level_group(scene).find("Floors")->groups[0].faces[0];
    double minX = 1e18, minY = 1e18;
    for (Vec3 p : floor.loops[0]) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
    }
    CHECK(minX == doctest::Approx(165));
    CHECK(minY == doctest::Approx(165));
}

TEST_CASE("geo-location and north") {
    Plan plan = test::fixture("01-rectangular-room");
    CHECK_FALSE(build_scene(plan).location);
    plan.orientation.lat = 39.7392;
    plan.orientation.lon = -104.9903;
    const Scene scene = build_scene(plan);
    REQUIRE(scene.location);
    CHECK(scene.location->lat == doctest::Approx(39.7392));
    CHECK(scene.northAngleDeg == doctest::Approx(17.5));
}

TEST_CASE("low-confidence scan items are flagged") {
    Plan plan = test::fixture("01-rectangular-room");
    test::wall(plan, "W2").confidence = 0.3;
    const Scene scene = build_scene(plan);
    CHECK(std::any_of(scene.warnings.begin(), scene.warnings.end(),
                      [](const std::string& w) { return w.find("W2") != std::string::npos; }));
}

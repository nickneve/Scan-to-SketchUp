#include <doctest/doctest.h>

#include <algorithm>

#include "helpers.hpp"
#include "rooms.hpp"

using namespace s2s;

TEST_CASE("room outlines sit on the inside faces") {
    SUBCASE("rectangle: 12 ft x 10 ft inside") {
        const Plan plan = test::fixture("01-rectangular-room");
        const RoomShape s = room_shape(plan.levels[0], plan.levels[0].rooms[0]);
        CHECK(s.interior.size() == 4);
        CHECK(s.area() == doctest::Approx(3658.0 * 3048.0));
    }
    SUBCASE("L-shape keeps six corners") {
        const Plan plan = test::fixture("02-l-shaped-room");
        const RoomShape s = room_shape(plan.levels[0], plan.levels[0].rooms[0]);
        CHECK(s.interior.size() == 6);
        CHECK(s.area() == doctest::Approx(4877.0 * 2438.0 + 2438.0 * 1829.0));
    }
    SUBCASE("open-plan room extends to the separator line") {
        const Plan plan = test::fixture("03-three-rooms-hallway");
        const Level& level = plan.levels[0];
        const RoomShape living = room_shape(level, *level.find_room("R1"));
        // Beside the wall stub (I1) the room stops at its face; past the stub it runs to the separator.
        CHECK(living.area() == doctest::Approx(4278.0 * 2835.0 + 4335.0 * 3261.0));
        bool jog = false;
        for (const RoomEdge& e : living.edges) jog |= e.wallId.empty();
        CHECK(jog);
    }
}

TEST_CASE("wall order around a room doesn't matter") {
    Plan plan = test::fixture("01-rectangular-room");
    Room& r = plan.levels[0].rooms[0];
    const double forward = room_shape(plan.levels[0], r).area();
    std::reverse(r.boundaryWallIds.begin(), r.boundaryWallIds.end());
    CHECK(room_shape(plan.levels[0], r).area() == doctest::Approx(forward));
}

TEST_CASE("rooms that don't close are rejected") {
    Plan plan = test::fixture("01-rectangular-room");
    plan.levels[0].rooms[0].boundaryWallIds = {"W1", "W3", "W2", "W4"};
    CHECK_THROWS_AS(room_shape(plan.levels[0], plan.levels[0].rooms[0]), PlanError);
}

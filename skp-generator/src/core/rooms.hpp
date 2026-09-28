// Room outlines on the inside faces of their walls.
#pragma once

#include <string>
#include <vector>

#include "plan.hpp"
#include "polygon.hpp"

namespace s2s {

// One edge of a room's inside outline, attributed to the wall whose face it lies on.
struct RoomEdge {
    Vec2 a, b;           // counter-clockwise around the room
    std::string wallId;  // empty for the short jog where a wall meets a separator
    bool separator = false;
    bool wallForward = true;  // true when a -> b runs in the wall's start -> end direction
};

struct RoomShape {
    std::string roomId;
    Loop2 centerline;        // counter-clockwise, through wall centerlines
    Loop2 interior;          // counter-clockwise, on inside faces; collinear points removed
    std::vector<RoomEdge> edges;  // in order around the room
    double area() const { return signed_area(interior); }
};

// Throws PlanError if the room's walls don't form a closed loop.
RoomShape room_shape(const Level& level, const Room& room);

}  // namespace s2s

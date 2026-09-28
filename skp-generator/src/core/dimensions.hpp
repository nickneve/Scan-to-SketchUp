// Blueprint-style dimensions for one level.
//
//  - Inside each room: the length of each inside face run (opposite equal sides shown once).
//  - Inside, for doors/windows on interior walls: a string locating each opening's edges,
//    on the side the door swings into.
//  - Outside, per straight run of exterior wall (outermost last): opening locations, wall
//    segments (where interior walls meet it), and the overall length.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "plan.hpp"
#include "rooms.hpp"
#include "scene.hpp"
#include "walls.hpp"

namespace s2s {

struct DimensionSpacing {
    double roomOffset = 450;         // inside face -> room dimension line
    double roomOpeningOffset = 200;  // inside face -> interior opening string
    double exteriorStep = 600;       // between exterior strings, starting one step out
};

std::vector<Dimension> build_dimensions(const Level& level, const std::map<std::string, WallFootprint>& footprints,
                                        const std::vector<RoomShape>& rooms, double z,
                                        const DimensionSpacing& spacing = {});

// Which side of each wall faces outside (unit plan normal). Uses the rooms each wall bounds,
// falling back to "away from the middle of the house" for walls bounding no room.
std::map<std::string, Vec2> outward_normals(const Level& level, const std::vector<RoomShape>& rooms);

}  // namespace s2s

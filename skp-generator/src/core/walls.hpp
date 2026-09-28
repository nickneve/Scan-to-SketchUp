// Wall footprints (with corner joins) and wall solids (with openings cut through).
#pragma once

#include <map>
#include <string>
#include <vector>

#include "plan.hpp"
#include "polygon.hpp"
#include "scene.hpp"

namespace s2s {

// Plan-view outline of one solid wall after joining it to its neighbours.
//  - L corners are mitred.
//  - At a T, the two collinear walls run through with a square cut at the node, and
//    the stem wall stops at the face of the through wall.
//  - Free ends are square.
//  - Three or more walls with no collinear pair meet at the node point.
struct WallFootprint {
    std::string wallId;
    Loop2 polygon;  // counter-clockwise
    size_t rightEdge = 0;  // polygon[rightEdge] -> polygon[rightEdge + 1] lies on the wall's right face
    size_t leftEdge = 0;   // ... on the wall's left face (walking start -> end)
};

// Footprints for every solid (non-separator) wall, keyed by wall id.
std::map<std::string, WallFootprint> compute_footprints(const Level& level);

// Height of each wall: its own height if set, else the tallest ceiling of the rooms it bounds,
// else the level's floor-to-ceiling height.
std::map<std::string, double> wall_heights(const Level& level);

// An opening's cut through a wall: u along the centerline from the wall start, z above the floor.
struct WallCut {
    std::string openingId;
    double u0, u1, z0, z1;
};

std::vector<WallCut> wall_cuts(const Level& level, const std::string& wallId);

// Range of u (distance along the centerline from the wall start) covered by a face of the wall.
struct FaceSpan {
    double u0, u1;
};
FaceSpan face_span(const Wall& wall, const WallFootprint& fp, bool left);

// Closed solid for one wall, from baseZ to baseZ + height, with every cut going straight
// through. Throws PlanError when an opening doesn't fit on both faces.
std::vector<Face> build_wall_solid(const Wall& wall, const WallFootprint& fp, double baseZ, double height,
                                   const std::vector<WallCut>& cuts);

// Plan-view pieces of the wall at height z (for floor-plan drawings): the footprint with
// every opening that spans z removed.
std::vector<Loop2> wall_section(const Wall& wall, const WallFootprint& fp, const std::vector<WallCut>& cuts,
                                double z);

}  // namespace s2s

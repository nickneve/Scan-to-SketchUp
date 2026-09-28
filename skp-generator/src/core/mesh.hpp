// Face-level helpers: triangulation, solid checks, extrusion.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "polygon.hpp"
#include "scene.hpp"

namespace s2s {

using Triangle = std::array<Vec3, 3>;

Vec3 face_normal(const Face& face);  // unit normal of the outer loop (Newell's method)
double face_area(const Face& face);
std::vector<Triangle> triangulate(const Face& face);

struct SolidReport {
    bool closed = false;  // every edge is used exactly once in each direction
    std::vector<std::string> problems;
    double volume = 0;  // signed; positive when faces point outward
};

// Checks that faces form a watertight, consistently oriented solid (SketchUp's "solid").
SolidReport check_solid(const std::vector<Face>& faces);

// Builds a face from a 2D region lying in the plane origin + u*e1 + v*e2.
Face face_from_region(const Region& region, Vec3 origin, Vec3 e1, Vec3 e2);

// Extrudes a 2D profile (in the plane origin + u*e1 + v*e2) along e1 x e2 by depth.
// The profile's outer loop must be counter-clockwise. Returns a closed solid.
std::vector<Face> extrude(const Region& profile, Vec3 origin, Vec3 e1, Vec3 e2, double depth);

// Axis-aligned box as a closed solid.
std::vector<Face> box(Vec3 min, Vec3 max);

}  // namespace s2s

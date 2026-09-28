// 2D polygon utilities used by wall, room and face construction.
#pragma once

#include <array>
#include <optional>
#include <vector>

#include "math.hpp"

namespace s2s {

using Loop2 = std::vector<Vec2>;

// A planar region: one outer loop (counter-clockwise) and zero or more holes (clockwise).
struct Region {
    Loop2 outer;
    std::vector<Loop2> holes;
};

// Axis-aligned rectangle [u0,u1] x [v0,v1].
struct Rect {
    double u0, v0, u1, v1;
};

double signed_area(const Loop2& loop);  // > 0 when counter-clockwise
double region_area(const Region& region);
bool point_in_polygon(Vec2 p, const Loop2& loop);  // even-odd rule; boundary is unspecified

// Intersection of the infinite lines p + s*d and q + t*e; nullopt when (nearly) parallel.
std::optional<Vec2> intersect_lines(Vec2 p, Vec2 d, Vec2 q, Vec2 e);

// Keep the part of the polygon where dot(n, x) <= c (Sutherland-Hodgman, one plane).
Loop2 clip_half_plane(const Loop2& loop, Vec2 n, double c);

// Drop repeated points and points lying on the segment between their neighbours.
Loop2 simplify(const Loop2& loop, double eps = 1e-6);

// base minus the union of cuts, as regions with holes. Everything is axis-aligned, so
// the result is exact. Output loops contain only true corners.
std::vector<Region> rect_minus_rects(const Rect& base, const std::vector<Rect>& cuts);

// Ear-clipping triangulation of a region with holes. Triangles are counter-clockwise.
std::vector<std::array<Vec2, 3>> triangulate(const Region& region);

}  // namespace s2s

#include "walls.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "mesh.hpp"

namespace s2s {

namespace {

constexpr double kNodeTolerance = 1.0;  // mm; endpoints closer than this are the same node
constexpr double kEps = 0.5;            // mm; openings closer than this to a limit are "at" it

struct End {
    size_t wall;
    bool atStart;
    Vec2 dir;  // pointing away from the node, along the wall
    double half;
};

struct Line {
    Vec2 p, d;
};

Line right_line(Vec2 node, const End& e) { return {node + right_of(e.dir) * e.half, e.dir}; }
Line left_line(Vec2 node, const End& e) { return {node + left_of(e.dir) * e.half, e.dir}; }

std::optional<Vec2> meet(const Line& a, const Line& b) { return intersect_lines(a.p, a.d, b.p, b.d); }

using Cap = std::vector<Vec2>;  // from the wall's right side to its left side (relative to End::dir)

Cap square_cap(Vec2 node, const End& e) { return {right_line(node, e).p, left_line(node, e).p}; }

std::vector<Cap> node_caps(Vec2 node, const std::vector<End>& ends) {
    const size_t k = ends.size();
    std::vector<Cap> caps(k);
    if (k == 1) {
        caps[0] = square_cap(node, ends[0]);
        return caps;
    }

    // Look for a pair of (nearly) collinear walls that run straight through the node.
    const double cosLimit = std::cos(2.0 * kPi / 180.0);
    int ta = -1, tb = -1;
    double best = -1;
    for (size_t a = 0; a < k; ++a)
        for (size_t b = a + 1; b < k; ++b)
            if (dot(ends[a].dir, ends[b].dir) < -cosLimit && ends[a].half + ends[b].half > best) {
                best = ends[a].half + ends[b].half;
                ta = static_cast<int>(a);
                tb = static_cast<int>(b);
            }

    if (ta >= 0) {
        const End& through = ends[ta];
        const double half = std::max(ends[ta].half, ends[tb].half);
        for (size_t i = 0; i < k; ++i) {
            if (static_cast<int>(i) == ta || static_cast<int>(i) == tb) {
                caps[i] = square_cap(node, ends[i]);
                continue;
            }
            // Stem: stop at the through wall's face on the stem's side.
            Vec2 side = cross(through.dir, ends[i].dir) > 0 ? left_of(through.dir) : right_of(through.dir);
            Line face{node + side * half, through.dir};
            auto r = meet(right_line(node, ends[i]), face);
            auto l = meet(left_line(node, ends[i]), face);
            caps[i] = (r && l) ? Cap{*r, *l} : square_cap(node, ends[i]);
        }
        return caps;
    }

    // No through pair: sort by angle and join each neighbouring pair where their faces meet.
    std::vector<size_t> order(k);
    for (size_t i = 0; i < k; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return std::atan2(ends[a].dir.y, ends[a].dir.x) < std::atan2(ends[b].dir.y, ends[b].dir.x);
    });
    std::vector<Vec2> corner(k);  // corner[i]: between order[i]'s left face and order[i+1]'s right face
    for (size_t i = 0; i < k; ++i) {
        const End& a = ends[order[i]];
        const End& b = ends[order[(i + 1) % k]];
        auto c = meet(left_line(node, a), right_line(node, b));
        const double limit = 6 * std::max(a.half, b.half);
        corner[i] = (c && distance(*c, node) <= limit) ? *c : left_line(node, a).p;
    }
    for (size_t i = 0; i < k; ++i) {
        const Vec2 right = corner[(i + k - 1) % k], left = corner[i];
        caps[order[i]] = k == 2 ? Cap{right, left} : Cap{right, node, left};
    }
    return caps;
}

double along(const Wall& w, Vec2 p) { return dot(p - w.start, w.direction()); }

}  // namespace

std::map<std::string, WallFootprint> compute_footprints(const Level& level) {
    std::vector<const Wall*> walls;
    for (const Wall& w : level.walls)
        if (!w.separator) walls.push_back(&w);

    std::vector<Vec2> nodes;
    std::vector<std::vector<End>> incident;
    auto node_of = [&](Vec2 p) {
        for (size_t i = 0; i < nodes.size(); ++i)
            if (distance(nodes[i], p) <= kNodeTolerance) return i;
        nodes.push_back(p);
        incident.emplace_back();
        return nodes.size() - 1;
    };

    std::vector<std::pair<size_t, size_t>> wallNodes;  // (start node, end node)
    for (size_t i = 0; i < walls.size(); ++i) {
        const Wall& w = *walls[i];
        const Vec2 d = w.direction();
        size_t s = node_of(w.start), e = node_of(w.end);
        incident[s].push_back({i, true, d, w.thickness / 2});
        incident[e].push_back({i, false, -d, w.thickness / 2});
        wallNodes.push_back({s, e});
    }

    std::vector<Cap> startCaps(walls.size()), endCaps(walls.size());
    for (size_t n = 0; n < nodes.size(); ++n) {
        std::vector<Cap> caps = node_caps(nodes[n], incident[n]);
        for (size_t i = 0; i < caps.size(); ++i) {
            const End& e = incident[n][i];
            (e.atStart ? startCaps : endCaps)[e.wall] = caps[i];
        }
    }

    std::map<std::string, WallFootprint> out;
    for (size_t i = 0; i < walls.size(); ++i) {
        const Cap& S = startCaps[i];  // right -> left relative to the wall direction
        const Cap& E = endCaps[i];    // left -> right relative to the wall direction
        WallFootprint fp;
        fp.wallId = walls[i]->id;
        fp.polygon.push_back(S.front());
        for (auto it = E.rbegin(); it != E.rend(); ++it) fp.polygon.push_back(*it);
        for (auto it = S.rbegin(); it + 1 != S.rend(); ++it) fp.polygon.push_back(*it);
        fp.rightEdge = 0;
        fp.leftEdge = E.size();
        if (signed_area(fp.polygon) <= 0)
            throw PlanError("wall " + fp.wallId + " collapses where it meets its neighbours; check its length");
        out[fp.wallId] = std::move(fp);
    }
    return out;
}

std::map<std::string, double> wall_heights(const Level& level) {
    std::map<std::string, double> h;
    for (const Wall& w : level.walls) h[w.id] = level.floorToCeiling;
    for (const Room& r : level.rooms) {
        double ceiling = r.ceilingHeight.value_or(level.floorToCeiling);
        for (const std::string& wid : r.boundaryWallIds) h[wid] = std::max(h[wid], ceiling);
    }
    for (const Wall& w : level.walls)
        if (w.height) h[w.id] = *w.height;
    return h;
}

std::vector<WallCut> wall_cuts(const Level& level, const std::string& wallId) {
    std::vector<WallCut> cuts;
    for (const Opening& o : level.openings)
        if (o.wallId == wallId)
            cuts.push_back({o.id, o.offsetFromStart, o.offsetFromStart + o.width, o.sillHeight,
                            o.sillHeight + o.height});
    std::sort(cuts.begin(), cuts.end(), [](const WallCut& a, const WallCut& b) { return a.u0 < b.u0; });
    return cuts;
}

FaceSpan face_span(const Wall& wall, const WallFootprint& fp, bool left) {
    size_t i = left ? fp.leftEdge : fp.rightEdge;
    double a = along(wall, fp.polygon[i]), b = along(wall, fp.polygon[(i + 1) % fp.polygon.size()]);
    return {std::min(a, b), std::max(a, b)};
}

namespace {

// Footprint pieces left after removing the given u-intervals (sorted, non-overlapping).
std::vector<Loop2> footprint_pieces(const Wall& wall, const WallFootprint& fp,
                                    const std::vector<std::pair<double, double>>& removed) {
    const Vec2 d = wall.direction();
    const double base = dot(d, wall.start);
    std::vector<Loop2> pieces;
    double lo = -std::numeric_limits<double>::infinity();
    for (size_t i = 0; i <= removed.size(); ++i) {
        double hi = i < removed.size() ? removed[i].first : std::numeric_limits<double>::infinity();
        Loop2 loop = fp.polygon;
        if (std::isfinite(hi)) loop = clip_half_plane(loop, d, hi + base);
        if (std::isfinite(lo) && loop.size() >= 3) loop = clip_half_plane(loop, -d, -(lo + base));
        if (loop.size() >= 3 && signed_area(loop) > 1e-6) pieces.push_back(loop);
        if (i < removed.size()) lo = removed[i].second;
    }
    return pieces;
}

}  // namespace

std::vector<Face> build_wall_solid(const Wall& wall, const WallFootprint& fp, double baseZ, double height,
                                   const std::vector<WallCut>& rawCuts) {
    const Vec2 d = wall.direction();
    const double half = wall.thickness / 2;
    const FaceSpan spanR = face_span(wall, fp, false), spanL = face_span(wall, fp, true);

    std::vector<WallCut> cuts = rawCuts;
    std::sort(cuts.begin(), cuts.end(), [](const WallCut& a, const WallCut& b) { return a.u0 < b.u0; });
    for (size_t i = 0; i < cuts.size(); ++i) {
        WallCut& c = cuts[i];
        const std::string what = "opening " + c.openingId + " on wall " + wall.id;
        if (c.u0 < std::max(spanR.u0, spanL.u0) + kEps || c.u1 > std::min(spanR.u1, spanL.u1) - kEps)
            throw PlanError(what + " runs into the corner; it must fit between the wall's inside corners");
        if (c.z1 > height + kEps) throw PlanError(what + " is taller than the wall");
        if (c.z0 <= kEps) c.z0 = 0;
        if (c.z1 >= height - kEps) c.z1 = height;
        if (i > 0 && c.u0 < cuts[i - 1].u1 + kEps)
            throw PlanError(what + " touches or overlaps opening " + cuts[i - 1].openingId);
    }

    std::vector<Face> faces;
    const Vec3 up{0, 0, 1};

    // Vertical faces: one per footprint edge; the two long faces get the openings.
    const size_t n = fp.polygon.size();
    for (size_t i = 0; i < n; ++i) {
        const Vec2 a = fp.polygon[i], b = fp.polygon[(i + 1) % n];
        const Vec3 a3 = to3(a, baseZ), b3 = to3(b, baseZ);
        if (i != fp.rightEdge && i != fp.leftEdge) {
            faces.push_back(Face{{{a3, b3, b3 + up * height, a3 + up * height}}, {}});
            continue;
        }
        const Vec2 dir = normalized(b - a);
        const bool forward = dot(dir, d) > 0;
        const double ua = along(wall, a);
        std::vector<Rect> holes;
        for (const WallCut& c : cuts) {
            double lo = forward ? c.u0 - ua : ua - c.u1;
            double hi = forward ? c.u1 - ua : ua - c.u0;
            holes.push_back({lo, c.z0, hi, c.z1});
        }
        for (const Region& r : rect_minus_rects({0, 0, distance(a, b), height}, holes))
            faces.push_back(face_from_region(r, a3, to3(dir), up));
    }

    // Top and bottom, split wherever an opening reaches them.
    std::vector<std::pair<double, double>> top, bottom;
    for (const WallCut& c : cuts) {
        if (c.z1 >= height) top.push_back({c.u0, c.u1});
        if (c.z0 <= 0) bottom.push_back({c.u0, c.u1});
    }
    for (const Loop2& piece : footprint_pieces(wall, fp, top)) {
        std::vector<Vec3> loop;
        for (Vec2 p : piece) loop.push_back(to3(p, baseZ + height));
        faces.push_back(Face{{loop}, {}});
    }
    for (const Loop2& piece : footprint_pieces(wall, fp, bottom)) {
        std::vector<Vec3> loop;
        for (auto it = piece.rbegin(); it != piece.rend(); ++it) loop.push_back(to3(*it, baseZ));
        faces.push_back(Face{{loop}, {}});
    }

    // Reveals: jambs, head and sill of each opening.
    auto R = [&](double u, double z) { return to3(wall.start + d * u + right_of(d) * half, baseZ + z); };
    auto L = [&](double u, double z) { return to3(wall.start + d * u + left_of(d) * half, baseZ + z); };
    for (const WallCut& c : cuts) {
        faces.push_back(Face{{{R(c.u0, c.z0), L(c.u0, c.z0), L(c.u0, c.z1), R(c.u0, c.z1)}}, {}});
        faces.push_back(Face{{{L(c.u1, c.z0), R(c.u1, c.z0), R(c.u1, c.z1), L(c.u1, c.z1)}}, {}});
        if (c.z1 < height) faces.push_back(Face{{{L(c.u0, c.z1), L(c.u1, c.z1), R(c.u1, c.z1), R(c.u0, c.z1)}}, {}});
        if (c.z0 > 0) faces.push_back(Face{{{R(c.u0, c.z0), R(c.u1, c.z0), L(c.u1, c.z0), L(c.u0, c.z0)}}, {}});
    }
    return faces;
}

std::vector<Loop2> wall_section(const Wall& wall, const WallFootprint& fp, const std::vector<WallCut>& cuts,
                                double z) {
    std::vector<std::pair<double, double>> removed;
    for (const WallCut& c : cuts)
        if (c.z0 < z && z < c.z1) removed.push_back({c.u0, c.u1});
    return footprint_pieces(wall, fp, removed);
}

}  // namespace s2s

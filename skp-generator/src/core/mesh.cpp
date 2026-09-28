#include "mesh.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace s2s {

Vec3 face_normal(const Face& face) {
    Vec3 n{};
    const auto& loop = face.loops.at(0);
    for (size_t i = 0; i < loop.size(); ++i) {
        const Vec3& a = loop[i];
        const Vec3& b = loop[(i + 1) % loop.size()];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return normalized(n);
}

namespace {

struct Basis {
    Vec3 origin, e1, e2;
    Vec2 to2(Vec3 p) const { return {dot(p - origin, e1), dot(p - origin, e2)}; }
    Vec3 to3(Vec2 p) const { return origin + e1 * p.x + e2 * p.y; }
};

Basis face_basis(const Face& face) {
    const auto& loop = face.loops.at(0);
    Vec3 n = face_normal(face);
    Vec3 e1{};
    for (size_t i = 0; i < loop.size() && length(e1) < 1e-9; ++i) e1 = loop[(i + 1) % loop.size()] - loop[i];
    e1 = normalized(e1);
    return {loop[0], e1, cross(n, e1)};
}

Region to_region(const Face& face, const Basis& b) {
    Region r;
    for (size_t i = 0; i < face.loops.size(); ++i) {
        Loop2 l;
        for (const Vec3& p : face.loops[i]) l.push_back(b.to2(p));
        if (i == 0) r.outer = std::move(l);
        else r.holes.push_back(std::move(l));
    }
    return r;
}

}  // namespace

double face_area(const Face& face) {
    Basis b = face_basis(face);
    return region_area(to_region(face, b));
}

std::vector<Triangle> triangulate(const Face& face) {
    Basis b = face_basis(face);
    std::vector<Triangle> out;
    for (const auto& t : triangulate(to_region(face, b))) out.push_back({b.to3(t[0]), b.to3(t[1]), b.to3(t[2])});
    return out;
}

SolidReport check_solid(const std::vector<Face>& faces) {
    using Key = std::tuple<long long, long long, long long>;
    auto key = [](Vec3 p) {
        return Key{std::llround(p.x * 100), std::llround(p.y * 100), std::llround(p.z * 100)};
    };
    std::map<std::pair<Key, Key>, int> directed;
    for (const Face& f : faces)
        for (const auto& loop : f.loops)
            for (size_t i = 0; i < loop.size(); ++i) ++directed[{key(loop[i]), key(loop[(i + 1) % loop.size()])}];

    SolidReport report;
    auto fmt = [](const Key& k) {
        return "(" + std::to_string(std::get<0>(k) / 100.0) + ", " + std::to_string(std::get<1>(k) / 100.0) + ", " +
               std::to_string(std::get<2>(k) / 100.0) + ")";
    };
    for (const auto& [edge, count] : directed) {
        auto rev = directed.find({edge.second, edge.first});
        int revCount = rev == directed.end() ? 0 : rev->second;
        if (count != 1 || revCount != 1) {
            if (report.problems.size() < 10)
                report.problems.push_back("edge " + fmt(edge.first) + " -> " + fmt(edge.second) + " used " +
                                          std::to_string(count) + "x forward, " + std::to_string(revCount) +
                                          "x reverse");
            else if (report.problems.size() == 10)
                report.problems.push_back("...");
        }
    }
    report.closed = report.problems.empty() && !faces.empty();

    for (const Face& f : faces)
        for (const Triangle& t : triangulate(f)) report.volume += dot(t[0], cross(t[1], t[2])) / 6.0;
    return report;
}

Face face_from_region(const Region& region, Vec3 origin, Vec3 e1, Vec3 e2) {
    Basis b{origin, e1, e2};
    Face f;
    auto lift = [&](const Loop2& l) {
        std::vector<Vec3> out;
        for (Vec2 p : l) out.push_back(b.to3(p));
        return out;
    };
    f.loops.push_back(lift(region.outer));
    for (const auto& h : region.holes) f.loops.push_back(lift(h));
    return f;
}

std::vector<Face> extrude(const Region& profile, Vec3 origin, Vec3 e1, Vec3 e2, double depth) {
    const Vec3 shift = normalized(cross(e1, e2)) * depth;
    std::vector<Face> faces;

    Face back = face_from_region(profile, origin, e1, e2);
    for (auto& loop : back.loops) std::reverse(loop.begin(), loop.end());
    faces.push_back(back);
    faces.push_back(face_from_region(profile, origin + shift, e1, e2));

    Basis b{origin, e1, e2};
    auto sides = [&](const Loop2& loop) {
        for (size_t i = 0; i < loop.size(); ++i) {
            Vec3 a = b.to3(loop[i]), c = b.to3(loop[(i + 1) % loop.size()]);
            faces.push_back(Face{{{a, c, c + shift, a + shift}}, {}});
        }
    };
    sides(profile.outer);
    for (const auto& h : profile.holes) sides(h);
    return faces;
}

std::vector<Face> box(Vec3 min, Vec3 max) {
    Region r{{{min.x, min.y}, {max.x, min.y}, {max.x, max.y}, {min.x, max.y}}, {}};
    return extrude(r, {0, 0, min.z}, {1, 0, 0}, {0, 1, 0}, max.z - min.z);
}

}  // namespace s2s

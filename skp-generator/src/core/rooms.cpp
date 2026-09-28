#include "rooms.hpp"

#include <algorithm>

namespace s2s {

namespace {

constexpr double kTolerance = 1.0;

bool near(Vec2 a, Vec2 b) { return distance(a, b) <= kTolerance; }

// The endpoint wall a shares with wall b.
std::optional<Vec2> shared_point(const Wall& a, const Wall& b) {
    for (Vec2 p : {a.start, a.end})
        for (Vec2 q : {b.start, b.end})
            if (near(p, q)) return p;
    return std::nullopt;
}

}  // namespace

RoomShape room_shape(const Level& level, const Room& room) {
    std::vector<const Wall*> walls;
    for (const std::string& id : room.boundaryWallIds) walls.push_back(level.find_wall(id));
    const size_t n = walls.size();

    // vertex[i] is where walls[i-1] meets walls[i]; edge i runs vertex[i] -> vertex[i+1].
    std::vector<Vec2> vertex(n);
    for (size_t i = 0; i < n; ++i) {
        auto p = shared_point(*walls[(i + n - 1) % n], *walls[i]);
        if (!p)
            throw PlanError("room " + room.id + " does not close: walls " + walls[(i + n - 1) % n]->id + " and " +
                            walls[i]->id + " don't share an endpoint");
        vertex[i] = *p;
    }
    if (signed_area(vertex) < 0) {
        std::reverse(walls.begin(), walls.end());
        std::vector<Vec2> v(n);
        for (size_t i = 0; i < n; ++i) v[i] = vertex[(n - i) % n];  // vertex between reversed neighbours
        vertex = v;
    }

    RoomShape shape;
    shape.roomId = room.id;
    shape.centerline = vertex;

    struct Offset {
        Vec2 p, d;
        double h;
    };
    std::vector<Offset> lines(n);
    for (size_t i = 0; i < n; ++i) {
        Vec2 d = normalized(vertex[(i + 1) % n] - vertex[i]);
        double h = walls[i]->separator ? 0 : walls[i]->thickness / 2;
        lines[i] = {vertex[i] + left_of(d) * h, d, h};
    }

    // Start and end of each wall's inside-face edge.
    std::vector<Vec2> first(n), last(n);
    for (size_t i = 0; i < n; ++i) {
        const Offset& a = lines[i];
        const Offset& b = lines[(i + 1) % n];
        const Vec2 corner = vertex[(i + 1) % n];
        if (auto p = intersect_lines(a.p, a.d, b.p, b.d)) {
            last[i] = *p;
            first[(i + 1) % n] = *p;
        } else {  // collinear walls: continue straight, or step where thicknesses differ
            last[i] = corner + left_of(a.d) * a.h;
            first[(i + 1) % n] = corner + left_of(b.d) * b.h;
        }
    }

    Loop2 outline;
    for (size_t i = 0; i < n; ++i) {
        RoomEdge e;
        e.a = first[i];
        e.b = last[i];
        e.wallId = walls[i]->id;
        e.separator = walls[i]->separator;
        e.wallForward = dot(e.b - e.a, walls[i]->end - walls[i]->start) > 0;
        shape.edges.push_back(e);
        outline.push_back(first[i]);
        outline.push_back(last[i]);
        const Vec2 next = first[(i + 1) % n];
        if (!near(last[i], next)) shape.edges.push_back({last[i], next, "", false, true});
    }
    shape.interior = simplify(outline, 1e-6);
    if (shape.interior.size() < 3 || signed_area(shape.interior) <= 0)
        throw PlanError("room " + room.id + " has no inside area; check its wall thicknesses");
    return shape;
}

}  // namespace s2s

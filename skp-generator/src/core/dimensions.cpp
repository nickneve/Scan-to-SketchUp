#include "dimensions.hpp"

#include <algorithm>
#include <cmath>

namespace s2s {

namespace {

constexpr double kTol = 1.0;
constexpr double kMinDimension = 150;  // skip runs shorter than this (jogs, slivers)

bool parallel(Vec2 a, Vec2 b) { return std::abs(cross(a, b)) < 1e-6; }

// A straight stretch of a room's inside outline, possibly spanning several walls.
struct Run {
    Vec2 a, b;
    std::vector<std::string> wallIds;
    bool solid = false;  // at least one real (non-separator) wall
    Vec2 dir() const { return normalized(b - a); }
    Vec2 inward() const { return left_of(dir()); }
};

std::vector<Run> room_runs(const RoomShape& shape) {
    const auto& edges = shape.edges;
    const size_t n = edges.size();
    auto continues = [&](const RoomEdge& p, const RoomEdge& q) {
        return distance(p.b, q.a) <= kTol && parallel(normalized(p.b - p.a), normalized(q.b - q.a)) &&
               dot(p.b - p.a, q.b - q.a) > 0;
    };
    size_t start = 0;
    for (size_t i = 0; i < n; ++i)
        if (!continues(edges[(i + n - 1) % n], edges[i])) {
            start = i;
            break;
        }

    std::vector<Run> runs;
    for (size_t k = 0; k < n; ++k) {
        const RoomEdge& e = edges[(start + k) % n];
        if (k > 0 && continues(edges[(start + k - 1) % n], e)) {
            runs.back().b = e.b;
        } else {
            runs.push_back({e.a, e.b, {}, false});
        }
        if (!e.wallId.empty()) runs.back().wallIds.push_back(e.wallId);
        if (!e.wallId.empty() && !e.separator) runs.back().solid = true;
    }
    return runs;
}

// How far the room extends inward from a run.
double room_depth(const RoomShape& shape, const Run& run) {
    double depth = 0;
    for (Vec2 p : shape.interior) depth = std::max(depth, dot(p - run.a, run.inward()));
    return depth;
}

Dimension make(Vec2 a, Vec2 b, Vec2 offset, double z) { return {to3(a, z), to3(b, z), to3(offset)}; }

// Dimensions between consecutive points along a line (points given as distances along dir from origin).
void add_string(std::vector<Dimension>& out, Vec2 origin, Vec2 dir, std::vector<double> ts, Vec2 offset, double z) {
    std::sort(ts.begin(), ts.end());
    ts.erase(std::unique(ts.begin(), ts.end(), [](double x, double y) { return std::abs(x - y) <= kTol; }), ts.end());
    for (size_t i = 0; i + 1 < ts.size(); ++i) out.push_back(make(origin + dir * ts[i], origin + dir * ts[i + 1], offset, z));
}

}  // namespace

std::map<std::string, Vec2> outward_normals(const Level& level, const std::vector<RoomShape>& rooms) {
    Vec2 centre{};
    int count = 0;
    for (const Wall& w : level.walls)
        if (!w.separator) {
            centre = centre + (w.start + w.end) * 0.5;
            ++count;
        }
    if (count) centre = centre * (1.0 / count);

    std::map<std::string, Vec2> out;
    for (const Wall& w : level.walls) {
        if (w.separator) continue;
        const Vec2 d = w.direction();
        std::optional<Vec2> n;
        for (const RoomShape& r : rooms)
            for (const RoomEdge& e : r.edges)
                if (e.wallId == w.id && !n) n = e.wallForward ? right_of(d) : left_of(d);  // away from the room
        if (!n) n = dot((w.start + w.end) * 0.5 - centre, left_of(d)) >= 0 ? left_of(d) : right_of(d);
        out[w.id] = *n;
    }
    return out;
}

std::vector<Dimension> build_dimensions(const Level& level, const std::map<std::string, WallFootprint>& footprints,
                                        const std::vector<RoomShape>& rooms, double z,
                                        const DimensionSpacing& spacing) {
    std::vector<Dimension> dims;

    // Inside each room.
    std::vector<std::vector<Run>> runsByRoom;
    for (const RoomShape& shape : rooms) {
        runsByRoom.push_back(room_runs(shape));
        std::vector<const Run*> shown;
        for (const Run& run : runsByRoom.back()) {
            const double len = distance(run.a, run.b);
            if (!run.solid || len < kMinDimension) continue;
            const bool duplicate = std::any_of(shown.begin(), shown.end(), [&](const Run* s) {
                if (!parallel(run.dir(), s->dir()) || std::abs(distance(s->a, s->b) - len) > 2 * kTol) return false;
                const double a0 = dot(run.a, run.dir()), a1 = dot(run.b, run.dir());
                const double b0 = dot(s->a, run.dir()), b1 = dot(s->b, run.dir());
                return std::abs(std::min(a0, a1) - std::min(b0, b1)) <= 2 * kTol &&
                       std::abs(std::max(a0, a1) - std::max(b0, b1)) <= 2 * kTol;
            });
            if (duplicate) continue;
            shown.push_back(&run);
            // Keep dimension lines near the wall in narrow rooms (hallways) so the room's
            // middle stays clear for its label.
            const double offset = std::min(spacing.roomOffset, room_depth(shape, run) * 0.25);
            dims.push_back(make(run.a, run.b, run.inward() * offset, z));
        }
    }

    // Openings in interior walls, located from inside the room they open into.
    std::map<std::pair<size_t, size_t>, std::vector<double>> strings;  // (room, run) -> positions
    for (const Opening& o : level.openings) {
        const Wall* w = level.find_wall(o.wallId);
        if (!w || w->separator || w->isExterior) continue;
        const bool wantLeft = o.swing ? o.swing->opensLeft : true;
        std::optional<std::pair<size_t, size_t>> pick;
        for (size_t r = 0; r < rooms.size(); ++r)
            for (const RoomEdge& e : rooms[r].edges) {
                if (e.wallId != w->id) continue;
                const bool roomOnLeft = e.wallForward;
                const auto& runs = runsByRoom[r];
                for (size_t k = 0; k < runs.size(); ++k)
                    if (std::find(runs[k].wallIds.begin(), runs[k].wallIds.end(), w->id) != runs[k].wallIds.end())
                        if (!pick || roomOnLeft == wantLeft) pick = {r, k};
            }
        if (!pick) continue;
        const Run& run = runsByRoom[pick->first][pick->second];
        const Vec2 d = w->direction();
        auto& ts = strings[*pick];
        for (double u : {o.offsetFromStart, o.offsetFromStart + o.width})
            ts.push_back(dot(w->start + d * u - run.a, run.dir()));
    }
    for (auto& [key, ts] : strings) {
        const Run& run = runsByRoom[key.first][key.second];
        ts.push_back(0);
        ts.push_back(distance(run.a, run.b));
        const double offset = std::min(spacing.roomOpeningOffset, room_depth(rooms[key.first], run) * 0.11);
        add_string(dims, run.a, run.dir(), ts, run.inward() * offset, z);
    }

    // Outside: group exterior faces into straight, continuous runs.
    const auto outward = outward_normals(level, rooms);
    struct ExteriorFace {
        const Wall* wall;
        Vec2 n;
        double c;       // dot(n, point on face)
        double t0, t1;  // extent along left_of(n)
    };
    std::vector<ExteriorFace> faces;
    for (const Wall& w : level.walls) {
        if (w.separator || !w.isExterior) continue;
        const WallFootprint& fp = footprints.at(w.id);
        const Vec2 n = outward.at(w.id);
        const bool left = dot(n, left_of(w.direction())) > 0;
        const size_t i = left ? fp.leftEdge : fp.rightEdge;
        const Vec2 p = fp.polygon[i], q = fp.polygon[(i + 1) % fp.polygon.size()];
        const Vec2 t = left_of(n);
        faces.push_back({&w, n, dot(n, p), std::min(dot(p, t), dot(q, t)), std::max(dot(p, t), dot(q, t))});
    }
    std::vector<bool> used(faces.size(), false);
    for (size_t i = 0; i < faces.size(); ++i) {
        if (used[i]) continue;
        std::vector<ExteriorFace> line;
        for (size_t j = i; j < faces.size(); ++j)
            if (!used[j] && dot(faces[j].n, faces[i].n) > 1 - 1e-9 && std::abs(faces[j].c - faces[i].c) <= kTol) {
                used[j] = true;
                line.push_back(faces[j]);
            }
        std::sort(line.begin(), line.end(), [](const ExteriorFace& a, const ExteriorFace& b) { return a.t0 < b.t0; });

        for (size_t s = 0; s < line.size();) {
            size_t e = s + 1;
            while (e < line.size() && line[e].t0 <= line[e - 1].t1 + kTol) ++e;
            const Vec2 n = line[s].n, dir = left_of(n), origin = n * line[s].c;

            std::vector<double> openingTs;
            std::vector<double> segmentTs{line[s].t0};
            for (size_t k = s; k < e; ++k) {
                segmentTs.push_back(line[k].t1);
                const Wall& w = *line[k].wall;
                for (const Opening& o : level.openings)
                    if (o.wallId == w.id)
                        for (double u : {o.offsetFromStart, o.offsetFromStart + o.width})
                            openingTs.push_back(dot(w.start + w.direction() * u, dir));
            }
            const double t0 = line[s].t0, t1 = line[e - 1].t1;
            int ring = 0;
            if (!openingTs.empty()) {
                openingTs.push_back(t0);
                openingTs.push_back(t1);
                add_string(dims, origin, dir, openingTs, n * (spacing.exteriorStep * ++ring), z);
            }
            if (e - s > 1) add_string(dims, origin, dir, segmentTs, n * (spacing.exteriorStep * ++ring), z);
            add_string(dims, origin, dir, {t0, t1}, n * (spacing.exteriorStep * ++ring), z);
            s = e;
        }
    }
    return dims;
}

}  // namespace s2s

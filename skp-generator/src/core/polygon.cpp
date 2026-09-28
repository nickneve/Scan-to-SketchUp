#include "polygon.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace s2s {

double signed_area(const Loop2& loop) {
    double a = 0;
    for (size_t i = 0, n = loop.size(); i < n; ++i) a += cross(loop[i], loop[(i + 1) % n]);
    return a / 2;
}

double region_area(const Region& region) {
    double a = std::abs(signed_area(region.outer));
    for (const auto& h : region.holes) a -= std::abs(signed_area(h));
    return a;
}

bool point_in_polygon(Vec2 p, const Loop2& loop) {
    bool inside = false;
    for (size_t i = 0, j = loop.size() - 1; i < loop.size(); j = i++) {
        const Vec2 a = loop[i], b = loop[j];
        if ((a.y > p.y) != (b.y > p.y)) {
            double x = a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (p.x < x) inside = !inside;
        }
    }
    return inside;
}

std::optional<Vec2> intersect_lines(Vec2 p, Vec2 d, Vec2 q, Vec2 e) {
    double denom = cross(d, e);
    if (std::abs(denom) < 1e-9 * length(d) * length(e)) return std::nullopt;
    double s = cross(q - p, e) / denom;
    return p + d * s;
}

Loop2 clip_half_plane(const Loop2& loop, Vec2 n, double c) {
    Loop2 out;
    const size_t count = loop.size();
    for (size_t i = 0; i < count; ++i) {
        Vec2 a = loop[i], b = loop[(i + 1) % count];
        double da = dot(n, a) - c, db = dot(n, b) - c;
        if (da <= 0) out.push_back(a);
        if ((da < 0 && db > 0) || (da > 0 && db < 0)) out.push_back(a + (b - a) * (da / (da - db)));
    }
    // Only drop repeated points: collinear vertices may be shared with neighbouring faces.
    Loop2 deduped;
    for (Vec2 p : out)
        if (deduped.empty() || distance(deduped.back(), p) > 1e-9) deduped.push_back(p);
    while (deduped.size() > 1 && distance(deduped.front(), deduped.back()) <= 1e-9) deduped.pop_back();
    return deduped;
}

Loop2 simplify(const Loop2& loop, double eps) {
    Loop2 pts;
    for (Vec2 p : loop)
        if (pts.empty() || distance(pts.back(), p) > eps) pts.push_back(p);
    while (pts.size() > 1 && distance(pts.front(), pts.back()) <= eps) pts.pop_back();

    bool changed = true;
    while (changed && pts.size() > 3) {
        changed = false;
        for (size_t i = 0; i < pts.size(); ++i) {
            Vec2 prev = pts[(i + pts.size() - 1) % pts.size()], cur = pts[i], next = pts[(i + 1) % pts.size()];
            Vec2 a = cur - prev, b = next - cur;
            if (std::abs(cross(a, b)) <= eps * length(a) * length(b) + eps * eps && dot(a, b) > 0) {
                pts.erase(pts.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
                break;
            }
        }
    }
    return pts;
}

namespace {

std::vector<double> sorted_unique(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    std::vector<double> out;
    for (double x : v)
        if (out.empty() || x - out.back() > 1e-9) out.push_back(x);
    return out;
}

}  // namespace

std::vector<Region> rect_minus_rects(const Rect& base, const std::vector<Rect>& cuts) {
    std::vector<double> xs{base.u0, base.u1}, ys{base.v0, base.v1};
    for (const Rect& c : cuts) {
        for (double u : {c.u0, c.u1}) xs.push_back(std::clamp(u, base.u0, base.u1));
        for (double v : {c.v0, c.v1}) ys.push_back(std::clamp(v, base.v0, base.v1));
    }
    xs = sorted_unique(xs);
    ys = sorted_unique(ys);
    const int nx = static_cast<int>(xs.size()) - 1, ny = static_cast<int>(ys.size()) - 1;

    auto filled = [&](int i, int j) {
        if (i < 0 || j < 0 || i >= nx || j >= ny) return false;
        double cx = (xs[i] + xs[i + 1]) / 2, cy = (ys[j] + ys[j + 1]) / 2;
        for (const Rect& c : cuts)
            if (c.u0 < cx && cx < c.u1 && c.v0 < cy && cy < c.v1) return false;
        return true;
    };

    // Directed boundary edges with the filled cell on their left, keyed by start vertex.
    using V = std::pair<int, int>;
    std::multimap<V, V> edges;
    for (int i = 0; i < nx; ++i)
        for (int j = 0; j < ny; ++j) {
            if (!filled(i, j)) continue;
            if (!filled(i, j - 1)) edges.insert({{i, j}, {i + 1, j}});
            if (!filled(i + 1, j)) edges.insert({{i + 1, j}, {i + 1, j + 1}});
            if (!filled(i, j + 1)) edges.insert({{i + 1, j + 1}, {i, j + 1}});
            if (!filled(i - 1, j)) edges.insert({{i, j + 1}, {i, j}});
        }

    std::vector<Loop2> outers, holes;
    while (!edges.empty()) {
        auto it = edges.begin();
        V start = it->first, prev = it->first, cur = it->second;
        edges.erase(it);
        Loop2 loop{{xs[start.first], ys[start.second]}};
        while (cur != start) {
            loop.push_back({xs[cur.first], ys[cur.second]});
            // At a pinch vertex prefer the leftmost turn so touching regions stay separate.
            auto range = edges.equal_range(cur);
            auto best = range.first;
            int bestScore = -1;
            for (auto e = range.first; e != range.second; ++e) {
                int dx0 = cur.first - prev.first, dy0 = cur.second - prev.second;
                int dx1 = e->second.first - cur.first, dy1 = e->second.second - cur.second;
                int turn = dx0 * dy1 - dy0 * dx1;
                int score = turn > 0 ? 2 : (turn == 0 ? 1 : 0);
                if (score > bestScore) {
                    bestScore = score;
                    best = e;
                }
            }
            prev = cur;
            cur = best->second;
            edges.erase(best);
        }
        loop = simplify(loop);
        (signed_area(loop) > 0 ? outers : holes).push_back(std::move(loop));
    }

    std::vector<Region> regions;
    for (auto& o : outers) regions.push_back({std::move(o), {}});
    for (auto& h : holes)
        for (auto& r : regions)
            if (point_in_polygon(h[0] + (h[1] - h[0]) * 0.5 + left_of(normalized(h[1] - h[0])) * -1e-6, r.outer)) {
                r.holes.push_back(std::move(h));
                break;
            }
    return regions;
}

namespace {

int orientation(Vec2 a, Vec2 b, Vec2 c) {
    double v = cross(b - a, c - a);
    double scale = std::max({length(b - a), length(c - a), 1.0});
    if (std::abs(v) <= 1e-9 * scale * scale) return 0;
    return v > 0 ? 1 : -1;
}

bool same(Vec2 a, Vec2 b) { return distance(a, b) <= 1e-9; }

bool segments_cross(Vec2 p1, Vec2 p2, Vec2 q1, Vec2 q2) {
    if (same(p1, q1) || same(p1, q2) || same(p2, q1) || same(p2, q2)) return false;
    int o1 = orientation(p1, p2, q1), o2 = orientation(p1, p2, q2);
    int o3 = orientation(q1, q2, p1), o4 = orientation(q1, q2, p2);
    return o1 * o2 < 0 && o3 * o4 < 0;
}

bool in_triangle(Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
    return orientation(a, b, p) >= 0 && orientation(b, c, p) >= 0 && orientation(c, a, p) >= 0;
}

// Joins each hole to the outer loop with a two-way bridge, producing one simple loop.
Loop2 bridge_holes(Loop2 outer, std::vector<Loop2> holes) {
    std::sort(holes.begin(), holes.end(), [](const Loop2& a, const Loop2& b) {
        auto mx = [](const Loop2& l) {
            double m = l[0].x;
            for (Vec2 p : l) m = std::max(m, p.x);
            return m;
        };
        return mx(a) > mx(b);
    });

    for (size_t h = 0; h < holes.size(); ++h) {
        const Loop2& hole = holes[h];
        size_t m = 0;
        for (size_t i = 1; i < hole.size(); ++i)
            if (hole[i].x > hole[m].x) m = i;
        Vec2 M = hole[m];

        std::vector<size_t> order(outer.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(),
                  [&](size_t a, size_t b) { return distance(outer[a], M) < distance(outer[b], M); });

        // A bridge must stay inside the material: no crossings, no vertices lying on it, and
        // sample points along it inside the outer loop and outside every remaining hole.
        auto blocked = [&](Vec2 V) {
            const double len = distance(M, V);
            auto hits = [&](const Loop2& l) {
                for (size_t i = 0; i < l.size(); ++i) {
                    if (segments_cross(M, V, l[i], l[(i + 1) % l.size()])) return true;
                    const Vec2 q = l[i];
                    if (same(q, M) || same(q, V)) continue;
                    const double t = dot(q - M, V - M) / (len * len);
                    if (t > 0 && t < 1 && distance(q, M + (V - M) * t) <= 1e-9 * std::max(len, 1.0)) return true;
                }
                return false;
            };
            if (hits(outer)) return true;
            for (size_t k = h; k < holes.size(); ++k)
                if (hits(holes[k])) return true;
            for (double t : {0.001, 0.5, 0.999}) {
                const Vec2 p = M + (V - M) * t;
                if (!point_in_polygon(p, outer)) return true;
                for (size_t k = h; k < holes.size(); ++k)
                    if (point_in_polygon(p, holes[k])) return true;
            }
            return false;
        };

        size_t vi = order[0];
        for (size_t idx : order)
            if (!blocked(outer[idx])) {
                vi = idx;
                break;
            }

        Loop2 merged(outer.begin(), outer.begin() + static_cast<std::ptrdiff_t>(vi) + 1);
        for (size_t k = 0; k <= hole.size(); ++k) merged.push_back(hole[(m + k) % hole.size()]);
        merged.insert(merged.end(), outer.begin() + static_cast<std::ptrdiff_t>(vi), outer.end());
        outer = std::move(merged);
    }
    return outer;
}

}  // namespace

std::vector<std::array<Vec2, 3>> triangulate(const Region& region) {
    Loop2 outer = region.outer;
    if (signed_area(outer) < 0) std::reverse(outer.begin(), outer.end());
    std::vector<Loop2> holes = region.holes;
    for (auto& h : holes)
        if (signed_area(h) > 0) std::reverse(h.begin(), h.end());

    Loop2 poly = bridge_holes(std::move(outer), std::move(holes));
    std::vector<std::array<Vec2, 3>> tris;
    std::vector<size_t> idx(poly.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;

    while (idx.size() > 3) {
        bool clipped = false;
        const size_t n = idx.size();
        for (size_t i = 0; i < n && !clipped; ++i) {
            Vec2 a = poly[idx[(i + n - 1) % n]], b = poly[idx[i]], c = poly[idx[(i + 1) % n]];
            if (orientation(a, b, c) <= 0) continue;
            bool ear = true;
            for (size_t k = 0; k < n && ear; ++k) {
                Vec2 p = poly[idx[k]];
                if (same(p, a) || same(p, b) || same(p, c)) continue;
                if (in_triangle(p, a, b, c)) ear = false;
            }
            if (ear) {
                tris.push_back({a, b, c});
                idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
                clipped = true;
            }
        }
        if (clipped) continue;
        // No ear: drop a degenerate (collinear) vertex if there is one, else stop.
        for (size_t i = 0; i < n && !clipped; ++i) {
            Vec2 a = poly[idx[(i + n - 1) % n]], b = poly[idx[i]], c = poly[idx[(i + 1) % n]];
            if (orientation(a, b, c) == 0) {
                idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
                clipped = true;
            }
        }
        if (!clipped) break;
    }
    if (idx.size() == 3 && orientation(poly[idx[0]], poly[idx[1]], poly[idx[2]]) > 0)
        tris.push_back({poly[idx[0]], poly[idx[1]], poly[idx[2]]});
    return tris;
}

}  // namespace s2s

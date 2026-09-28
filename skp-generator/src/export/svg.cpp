#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>

#include "build.hpp"
#include "exporters.hpp"
#include "flatten.hpp"
#include "rooms.hpp"
#include "units.hpp"
#include "walls.hpp"

namespace s2s {

namespace {

constexpr double kCutHeight = 1219;  // plan cut at 4 ft, like an architectural floor plan

std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else if (c == '"') o += "&quot;";
        else o += c;
    }
    return o;
}

struct View {
    double minX, maxY, scale, margin;
    Vec2 map(Vec2 p) const { return {(p.x - minX) * scale + margin, (maxY - p.y) * scale + margin}; }
};

std::string f(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", v);
    return buf;
}

std::string pts(const View& v, const std::vector<Vec2>& loop) {
    std::string s;
    for (Vec2 p : loop) {
        Vec2 q = v.map(p);
        s += f(q.x) + "," + f(q.y) + " ";
    }
    return s;
}

// A point well inside the polygon, for labels (coarse pole of inaccessibility).
Vec2 label_point(const Loop2& poly) {
    double x0 = poly[0].x, x1 = x0, y0 = poly[0].y, y1 = y0;
    for (Vec2 p : poly) {
        x0 = std::min(x0, p.x);
        x1 = std::max(x1, p.x);
        y0 = std::min(y0, p.y);
        y1 = std::max(y1, p.y);
    }
    Vec2 best{(x0 + x1) / 2, (y0 + y1) / 2};
    double bestD = -1;
    for (int i = 1; i < 30; ++i)
        for (int j = 1; j < 30; ++j) {
            Vec2 p{x0 + (x1 - x0) * i / 30, y0 + (y1 - y0) * j / 30};
            if (!point_in_polygon(p, poly)) continue;
            double d = std::numeric_limits<double>::infinity();
            for (size_t k = 0; k < poly.size(); ++k) {
                Vec2 a = poly[k], b = poly[(k + 1) % poly.size()];
                double t = std::clamp(dot(p - a, b - a) / std::max(dot(b - a, b - a), 1e-9), 0.0, 1.0);
                d = std::min(d, distance(p, a + (b - a) * t));
            }
            // Prefer central points: mild penalty for distance from the bbox centre.
            d -= 0.05 * distance(p, {(x0 + x1) / 2, (y0 + y1) / 2});
            if (d > bestD) {
                bestD = d;
                best = p;
            }
        }
    return best;
}

}  // namespace

std::string plan_svg(const Plan& input, const Scene& scene, size_t levelIndex) {
    const Plan plan = normalize_origin(input);
    const Level& level = plan.levels.at(levelIndex);
    const Group* levelGroup = scene.groups.at(0).groups.size() > levelIndex ? &scene.groups[0].groups[levelIndex] : nullptr;
    const auto footprints = compute_footprints(level);
    std::vector<RoomShape> shapes;
    for (const Room& r : level.rooms) shapes.push_back(room_shape(level, r));

    std::vector<Dimension> dims;
    if (levelGroup)
        if (const Group* g = levelGroup->find("Dimensions")) dims = g->dimensions;

    // Extents: walls plus dimension lines plus room for text.
    double minX = std::numeric_limits<double>::infinity(), minY = minX, maxX = -minX, maxY = -minX;
    auto grow = [&](Vec2 p) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    };
    for (const auto& [id, fp] : footprints)
        for (Vec2 p : fp.polygon) grow(p);
    for (const Dimension& d : dims) {
        grow(to2(d.start + d.offset) + normalized(to2(d.offset)) * 250);
        grow(to2(d.end + d.offset) + normalized(to2(d.offset)) * 250);
    }
    const double margin = 60;
    const double scale = std::min(1100.0 / (maxX - minX), 800.0 / (maxY - minY));
    const View v{minX, maxY, scale, margin};
    const double width = (maxX - minX) * scale + 2 * margin, height = (maxY - minY) * scale + 2 * margin + 40;

    std::ostringstream s;
    s << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << f(width) << " " << f(height) << "\" width=\""
      << f(width) << "\" height=\"" << f(height) << "\" font-family=\"Segoe UI, Helvetica, Arial, sans-serif\">\n";
    s << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";
    s << "<text x=\"" << f(margin) << "\" y=\"" << f(height - 18) << "\" font-size=\"13\" fill=\"#555\">"
      << esc("Level " + std::to_string(levelIndex + 1) + " - " + level.name) << " · plan cut at 4'-0\" · generated preview (not to scale)</text>\n";

    // Rooms.
    s << "<g id=\"rooms\">\n";
    for (size_t i = 0; i < shapes.size(); ++i)
        s << "<polygon points=\"" << pts(v, shapes[i].interior) << "\" fill=\"#f3eee4\" stroke=\"none\"/>\n";
    s << "</g>\n";

    // Walls cut at 4 ft; separators dashed.
    s << "<g id=\"walls\" fill=\"#2f2f2f\" stroke=\"#2f2f2f\" stroke-width=\"0.5\">\n";
    for (const Wall& w : level.walls) {
        if (w.separator) {
            Vec2 a = v.map(w.start), b = v.map(w.end);
            s << "<line x1=\"" << f(a.x) << "\" y1=\"" << f(a.y) << "\" x2=\"" << f(b.x) << "\" y2=\"" << f(b.y)
              << "\" stroke=\"#9a9a9a\" stroke-width=\"1\" stroke-dasharray=\"6 4\"/>\n";
            continue;
        }
        for (const Loop2& piece : wall_section(w, footprints.at(w.id), wall_cuts(level, w.id), kCutHeight))
            s << "<polygon points=\"" << pts(v, piece) << "\"/>\n";
    }
    s << "</g>\n";

    // Windows: frame outline with a glass line.
    s << "<g id=\"windows\" fill=\"#ffffff\" stroke=\"#2f2f2f\" stroke-width=\"1\">\n";
    for (const Opening& o : level.openings) {
        if (o.type != OpeningType::Window) continue;
        const Wall& w = *level.find_wall(o.wallId);
        const Vec2 d = w.direction(), l = left_of(d) * (w.thickness / 2);
        const Vec2 a = w.start + d * o.offsetFromStart, b = a + d * o.width;
        s << "<polygon points=\"" << pts(v, {a - l, b - l, b + l, a + l}) << "\"/>\n";
        Vec2 p = v.map(a), q = v.map(b);
        s << "<line x1=\"" << f(p.x) << "\" y1=\"" << f(p.y) << "\" x2=\"" << f(q.x) << "\" y2=\"" << f(q.y) << "\"/>\n";
    }
    s << "</g>\n";

    // Door swings from the placed components; other doors get a line across the opening.
    s << "<g id=\"doors\" fill=\"none\" stroke=\"#2f2f2f\" stroke-width=\"1\">\n";
    for (const FlatPart& part : flatten(scene, true))
        if (part.tag == tags::DoorSwings && part.path.rfind(scene.groups[0].name + "/" + (levelGroup ? levelGroup->name : ""), 0) == 0)
            for (const auto& line : part.lines) {
                std::vector<Vec2> l2;
                for (Vec3 p : line) l2.push_back(to2(p));
                s << "<polyline points=\"" << pts(v, l2) << "\"/>\n";
            }
    for (const Opening& o : level.openings) {
        if (o.type != OpeningType::Door || o.operation.empty() || o.operation == "hinged" || o.operation == "double") continue;
        const Wall& w = *level.find_wall(o.wallId);
        const Vec2 d = w.direction();
        Vec2 a = v.map(w.start + d * o.offsetFromStart), b = v.map(w.start + d * (o.offsetFromStart + o.width));
        s << "<line x1=\"" << f(a.x) << "\" y1=\"" << f(a.y) << "\" x2=\"" << f(b.x) << "\" y2=\"" << f(b.y)
          << "\" stroke-width=\"2\"/>\n";
    }
    s << "</g>\n";

    // Dimensions.
    s << "<g id=\"dimensions\" stroke=\"#1f5fa8\" stroke-width=\"0.8\" fill=\"#1f5fa8\" font-size=\"11\">\n";
    for (const Dimension& d : dims) {
        const Vec2 p1 = to2(d.start), p2 = to2(d.end), off = to2(d.offset);
        const Vec2 a = v.map(p1 + off), b = v.map(p2 + off), e1 = v.map(p1), e2 = v.map(p2);
        const Vec2 dir = normalized(b - a), n = normalized(a - e1);
        const Vec2 g1 = e1 + n * 3, g2 = e2 + n * 3, x1 = a + n * 4, x2 = b + n * 4;
        s << "<path d=\"M" << f(g1.x) << " " << f(g1.y) << "L" << f(x1.x) << " " << f(x1.y) << "M" << f(g2.x) << " "
          << f(g2.y) << "L" << f(x2.x) << " " << f(x2.y) << "M" << f(a.x) << " " << f(a.y) << "L" << f(b.x) << " "
          << f(b.y) << "\" fill=\"none\"/>\n";
        for (Vec2 c : {a, b}) {  // architectural tick
            Vec2 t = (dir + left_of(dir)) * 3.5;
            s << "<line x1=\"" << f(c.x - t.x) << "\" y1=\"" << f(c.y - t.y) << "\" x2=\"" << f(c.x + t.x) << "\" y2=\""
              << f(c.y + t.y) << "\" stroke-width=\"1.4\"/>\n";
        }
        double angle = std::atan2(b.y - a.y, b.x - a.x) * 180 / kPi;
        if (angle > 90.5) angle -= 180;
        if (angle <= -89.5) angle += 180;
        const Vec2 mid = (a + b) * 0.5;
        const double rad = angle * kPi / 180;
        const Vec2 up{std::sin(rad), -std::cos(rad)};
        const Vec2 tp = mid + up * 3;
        s << "<text x=\"" << f(tp.x) << "\" y=\"" << f(tp.y) << "\" text-anchor=\"middle\" stroke=\"none\" transform=\"rotate("
          << f(angle) << " " << f(tp.x) << " " << f(tp.y) << ")\">" << esc(format_length(distance(p1, p2), scene.units))
          << "</text>\n";
    }
    s << "</g>\n";

    // Room labels.
    s << "<g id=\"labels\" text-anchor=\"middle\" fill=\"#333\">\n";
    for (size_t i = 0; i < shapes.size(); ++i) {
        const Vec2 c = v.map(label_point(shapes[i].interior));
        s << "<text x=\"" << f(c.x) << "\" y=\"" << f(c.y - 2) << "\" font-size=\"14\" font-weight=\"600\">"
          << esc(level.rooms[i].name) << "</text>\n";
        s << "<text x=\"" << f(c.x) << "\" y=\"" << f(c.y + 14) << "\" font-size=\"11\" fill=\"#666\">"
          << esc(format_area(shapes[i].area(), scene.units)) << "</text>\n";
    }
    s << "</g>\n";

    // North arrow.
    const double a = plan.orientation.northAngleDeg * kPi / 180;
    const Vec2 c{width - 45, 45}, nd{std::sin(a), -std::cos(a)};
    const Vec2 tip = c + nd * 22, tail = c - nd * 22, wing = left_of(nd) * 7;
    s << "<g id=\"north\"><circle cx=\"" << f(c.x) << "\" cy=\"" << f(c.y) << "\" r=\"28\" fill=\"none\" stroke=\"#999\"/>"
      << "<polygon points=\"" << f(tip.x) << "," << f(tip.y) << " " << f(tail.x + wing.x) << "," << f(tail.y + wing.y)
      << " " << f(c.x) << "," << f(c.y) << " " << f(tail.x - wing.x) << "," << f(tail.y - wing.y)
      << "\" fill=\"#333\"/><text x=\"" << f(tip.x + nd.x * 12) << "\" y=\"" << f(tip.y + nd.y * 12 + 4)
      << "\" font-size=\"12\" font-weight=\"700\" text-anchor=\"middle\">N</text></g>\n";

    s << "</svg>\n";
    return s.str();
}

}  // namespace s2s

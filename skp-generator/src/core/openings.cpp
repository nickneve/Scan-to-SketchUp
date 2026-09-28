#include "openings.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>

#include "mesh.hpp"
#include "units.hpp"

namespace s2s {

namespace {

constexpr double kDoorFrame = 19;    // 3/4" jamb
constexpr double kWindowFrame = 45;  // window frame face width
constexpr double kLeafThickness = 44;  // 1-3/4" door
constexpr double kGlassThickness = 6;
constexpr double kClearance = 3;
constexpr double kUndercut = 13;

double round_to(double v, double step) { return std::round(v / step) * step; }

// Frame lining the opening, extruded through the wall (y from 0 to thickness).
Group frame_group(const Region& profile, double thickness) {
    Group g;
    g.name = "Frame";
    g.faces = extrude(profile, {0, thickness, 0}, {1, 0, 0}, {0, 0, 1}, thickness);
    return g;
}

Region u_profile(double w, double h, double f) {
    return {{{0, 0}, {f, 0}, {f, h - f}, {w - f, h - f}, {w - f, 0}, {w, 0}, {w, h}, {0, h}}, {}};
}

Group leaf_group(double x0, double x1, double thickness, double z0, double z1) {
    const double t = std::min(kLeafThickness, thickness - 2 * kClearance);
    Group g;
    g.name = "Leaf";
    g.faces = box({x0, thickness / 2 - t / 2, z0}, {x1, thickness / 2 + t / 2, z1});
    return g;
}

// Plan-view swing: the open leaf as a line and its path as a quarter arc, at floor level.
void add_swing(Group& swing, Vec3 hinge, double radius, bool hingeOnLeft) {
    const Vec3 open = hinge + Vec3{0, radius, 0};
    swing.edges.push_back({hinge, open});
    Arc arc;
    arc.center = hinge;
    arc.xaxis = {1, 0, 0};
    arc.normal = {0, 0, 1};
    arc.radius = radius;
    arc.startAngle = hingeOnLeft ? 0 : kPi / 2;
    arc.endAngle = hingeOnLeft ? kPi / 2 : kPi;
    arc.segments = 12;
    swing.arcs.push_back(arc);
}

}  // namespace

std::string OpeningStyle::base_name(DisplayUnits units) const {
    std::string kind;
    switch (type) {
        case OpeningType::Window: kind = "Window"; break;
        case OpeningType::Opening: kind = "Opening"; break;
        case OpeningType::Door:
            if (variant == "double") kind = "Double Door";
            else if (variant == "sliding") kind = "Sliding Door";
            else if (variant == "pocket") kind = "Pocket Door";
            else if (variant == "bifold") kind = "Bifold Door";
            else kind = "Door";
            break;
    }
    return kind + " " + std::to_string(nominal_size(width, units)) + "x" + std::to_string(nominal_size(height, units));
}

bool OpeningStyle::operator<(const OpeningStyle& o) const {
    return std::tie(type, variant, width, height, thickness) < std::tie(o.type, o.variant, o.width, o.height, o.thickness);
}

OpeningStyle opening_style(const Opening& opening, const Wall& wall) {
    OpeningStyle s{opening.type, "", round_to(opening.width, 0.1), round_to(opening.height, 0.1),
                   round_to(wall.thickness, 0.1)};
    if (opening.type == OpeningType::Door) {
        const std::string& op = opening.operation;
        s.variant = (op == "double" || op == "sliding" || op == "pocket" || op == "bifold") ? op : "hinged";
    }
    return s;
}

std::map<OpeningStyle, std::string> name_styles(const std::vector<OpeningStyle>& styles, DisplayUnits units) {
    std::vector<OpeningStyle> unique = styles;
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end(),
                             [](const OpeningStyle& a, const OpeningStyle& b) { return !(a < b) && !(b < a); }),
                 unique.end());

    std::map<std::string, int> baseCount;
    for (const auto& s : unique) ++baseCount[s.base_name(units)];

    std::map<OpeningStyle, std::string> names;
    std::map<std::string, int> used;
    for (const auto& s : unique) {
        std::string name = s.base_name(units);
        if (baseCount[name] > 1) name += " (" + format_thickness(s.thickness, units) + " wall)";
        if (int n = ++used[name]; n > 1) name += " #" + std::to_string(n);
        names[s] = name;
    }
    return names;
}

ComponentDefinition build_definition(const OpeningStyle& s, const std::string& name) {
    const double w = s.width, h = s.height, t = s.thickness;
    ComponentDefinition def;
    def.name = name;
    Group& c = def.contents;

    if (s.type == OpeningType::Window) {
        const double f = std::min(kWindowFrame, std::min(w, h) / 4);
        Region ring{{{0, 0}, {w, 0}, {w, h}, {0, h}}, {{{f, f}, {f, h - f}, {w - f, h - f}, {w - f, f}}}};
        c.groups.push_back(frame_group(ring, t));
        Group glass;
        glass.name = "Glass";
        glass.faces = box({f, t / 2 - kGlassThickness / 2, f}, {w - f, t / 2 + kGlassThickness / 2, h - f});
        for (Face& face : glass.faces) face.material = "Glass";
        c.groups.push_back(glass);
        def.description = "Window, frame and glass";
        return def;
    }

    const double f = std::min(kDoorFrame, w / 6);
    c.groups.push_back(frame_group(u_profile(w, h, f), t));
    if (s.type == OpeningType::Opening) {
        def.description = "Cased opening";
        return def;
    }

    const double top = h - f - kClearance;
    Group swing;
    swing.name = "Swing";
    swing.tag = "Door Swings";
    if (s.variant == "double") {
        const double mid = w / 2;
        c.groups.push_back(leaf_group(f + kClearance, mid - kClearance / 2, t, kUndercut, top));
        c.groups.back().name = "Leaf Left";
        c.groups.push_back(leaf_group(mid + kClearance / 2, w - f - kClearance, t, kUndercut, top));
        c.groups.back().name = "Leaf Right";
        add_swing(swing, {f, t, 0}, mid - f, true);
        add_swing(swing, {w - f, t, 0}, mid - f, false);
        c.groups.push_back(swing);
        def.description = "Double door, frame and leaves";
    } else {
        c.groups.push_back(leaf_group(f + kClearance, w - f - kClearance, t, kUndercut, top));
        if (s.variant == "hinged") {
            add_swing(swing, {f, t, 0}, w - 2 * f, true);
            c.groups.push_back(swing);
        }
        def.description = s.variant == "hinged" ? "Door, frame and leaf" : "Door (" + s.variant + "), frame and leaf";
    }
    return def;
}

Transform opening_transform(const Opening& o, const Wall& wall, double levelElevation) {
    const Vec2 d = wall.direction();
    const Vec3 X = to3(d), Y = to3(left_of(d)), Z{0, 0, 1};
    const Vec3 base = to3(wall.start + d * o.offsetFromStart + right_of(d) * (wall.thickness / 2),
                          levelElevation + o.sillHeight);

    double sx = 1, sy = 1;
    if (o.type == OpeningType::Door && o.swing) {
        sx = o.swing->hingeAtStart ? 1 : -1;
        sy = o.swing->opensLeft ? 1 : -1;
    }
    Transform t;
    t.origin = base + (sx < 0 ? X * o.width : Vec3{}) + (sy < 0 ? Y * wall.thickness : Vec3{});
    t.xaxis = X * sx;
    t.yaxis = Y * sy;
    t.zaxis = Z;
    return t;
}

}  // namespace s2s

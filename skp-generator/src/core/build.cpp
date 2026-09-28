#include "build.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>

#include "dimensions.hpp"
#include "openings.hpp"
#include "rooms.hpp"
#include "units.hpp"
#include "walls.hpp"

namespace s2s {

namespace {

constexpr double kLowConfidence = 0.5;

// Appends " 1", " 2", ... to every name that occurs more than once.
void number_duplicates(std::vector<std::string>& names) {
    std::map<std::string, int> total, seen;
    for (const auto& n : names) ++total[n];
    for (auto& n : names)
        if (total[n] > 1) n += " " + std::to_string(++seen[n]);
}

std::string join(const std::vector<std::string>& parts, const char* sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) out += (i ? sep : "") + parts[i];
    return out;
}

// "Wall - Office North" for exterior walls, "Wall - Bedroom / Hallway" for interior walls.
std::map<std::string, std::string> wall_names(const Level& level, const std::vector<RoomShape>& shapes,
                                              const std::map<std::string, Vec2>& outward, double north) {
    std::vector<const Wall*> walls;
    std::vector<std::string> names;
    for (const Wall& w : level.walls) {
        if (w.separator) continue;
        std::vector<std::string> rooms;
        for (const RoomShape& s : shapes)
            for (const RoomEdge& e : s.edges)
                if (e.wallId == w.id) {
                    const std::string& name = level.find_room(s.roomId)->name;
                    if (std::find(rooms.begin(), rooms.end(), name) == rooms.end()) rooms.push_back(name);
                }
        const std::string side = compass_name(outward.at(w.id), north);
        std::string name;
        if (w.isExterior) name = "Wall - " + (rooms.empty() ? std::string("Exterior") : join(rooms, " / ")) + " " + side;
        else if (rooms.size() >= 2) {
            std::sort(rooms.begin(), rooms.end());
            name = "Wall - " + join(rooms, " / ");
        } else if (rooms.size() == 1) name = "Wall - " + rooms[0] + " " + side;
        else name = "Wall - " + w.id;
        walls.push_back(&w);
        names.push_back(name);
    }
    number_duplicates(names);
    std::map<std::string, std::string> out;
    for (size_t i = 0; i < walls.size(); ++i) out[walls[i]->id] = names[i];
    return out;
}

std::string fmt_confidence(double c) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.2f", c);
    return buf;
}

Group build_level(const Level& level, int index, const Plan& plan, const std::map<OpeningStyle, std::string>& styleNames,
                  std::vector<std::string>& warnings) {
    const double z = level.elevation;
    const auto footprints = compute_footprints(level);
    const auto heights = wall_heights(level);

    std::vector<RoomShape> shapes;
    for (const Room& r : level.rooms) shapes.push_back(room_shape(level, r));
    const auto outward = outward_normals(level, shapes);
    const auto names = wall_names(level, shapes, outward, plan.orientation.northAngleDeg);

    Group lg;
    lg.name = "Level " + std::to_string(index + 1) + " - " + level.name;
    lg.attributes = {{"id", level.id}, {"type", "level"}};

    Group walls{"Walls"};
    for (const Wall& w : level.walls) {
        if (w.separator) continue;
        Group g;
        g.name = names.at(w.id);
        g.tag = tags::Walls;
        g.faces = build_wall_solid(w, footprints.at(w.id), z, heights.at(w.id), wall_cuts(level, w.id));
        g.attributes = {{"id", w.id}, {"type", "wall"}, {"isExterior", w.isExterior ? "true" : "false"}};
        walls.groups.push_back(std::move(g));
        if (w.confidence && *w.confidence < kLowConfidence)
            warnings.push_back("wall " + w.id + " (" + names.at(w.id) + ") has low scan confidence " +
                               fmt_confidence(*w.confidence) + "; worth measuring");
    }

    std::vector<std::string> floorNames, ceilingNames;
    for (const Room& r : level.rooms) {
        floorNames.push_back("Floor - " + r.name);
        ceilingNames.push_back("Ceiling - " + r.name);
    }
    number_duplicates(floorNames);
    number_duplicates(ceilingNames);

    Group floors{"Floors"}, ceilings{"Ceilings"};
    for (size_t i = 0; i < level.rooms.size(); ++i) {
        const Room& r = level.rooms[i];
        const RoomShape& s = shapes[i];
        std::vector<Vec3> up, down;
        for (Vec2 p : s.interior) up.push_back(to3(p, z));
        const double ceiling = z + r.ceilingHeight.value_or(level.floorToCeiling);
        for (auto it = s.interior.rbegin(); it != s.interior.rend(); ++it) down.push_back(to3(*it, ceiling));

        Group f;
        f.name = floorNames[i];
        f.tag = tags::Floors;
        f.faces.push_back(Face{{up}, {}});
        f.attributes = {{"id", r.id}, {"type", "floor"}, {"roomType", r.type}};
        floors.groups.push_back(std::move(f));

        Group c;
        c.name = ceilingNames[i];
        c.tag = tags::Ceilings;
        c.faces.push_back(Face{{down}, {}});
        c.attributes = {{"id", r.id}, {"type", "ceiling"}, {"roomType", r.type}};
        ceilings.groups.push_back(std::move(c));
    }

    Group doors{"Doors"}, windows{"Windows"};
    for (const Opening& o : level.openings) {
        const Wall& w = *level.find_wall(o.wallId);
        Instance inst;
        inst.definition = styleNames.at(opening_style(o, w));
        inst.transform = opening_transform(o, w, z);
        inst.attributes = {{"id", o.id}, {"type", o.type == OpeningType::Window ? "window" : "door"}, {"wallId", w.id}};
        if (o.type == OpeningType::Window) {
            inst.tag = tags::Windows;
            windows.instances.push_back(std::move(inst));
        } else {
            inst.tag = tags::Doors;
            doors.instances.push_back(std::move(inst));
        }
        const std::string style = opening_style(o, w).variant;
        if (o.type == OpeningType::Door && style == "hinged" && !o.swing)
            warnings.push_back("door " + o.id + " has no swing; drawn hinged at the wall-start side, opening to the left");
        if (o.confidence && *o.confidence < kLowConfidence)
            warnings.push_back("opening " + o.id + " has low scan confidence " + fmt_confidence(*o.confidence) +
                               "; worth measuring");
    }

    Group dims{"Dimensions"};
    dims.tag = tags::Dimensions;
    dims.dimensions = build_dimensions(level, footprints, shapes, z);

    for (Group* g : {&walls, &floors, &ceilings, &doors, &windows, &dims})
        if (!g->groups.empty() || !g->instances.empty() || !g->dimensions.empty()) lg.groups.push_back(std::move(*g));
    return lg;
}

}  // namespace

Plan normalize_origin(const Plan& plan) {
    double minX = std::numeric_limits<double>::infinity(), minY = minX;
    for (const Level& level : plan.levels)
        for (const auto& [id, fp] : compute_footprints(level))
            for (Vec2 p : fp.polygon) {
                minX = std::min(minX, p.x);
                minY = std::min(minY, p.y);
            }
    if (!std::isfinite(minX) || (std::abs(minX) < 0.01 && std::abs(minY) < 0.01)) return plan;

    Plan out = plan;
    const Vec2 shift{-minX, -minY};
    for (Level& level : out.levels)
        for (Wall& w : level.walls) {
            w.start = w.start + shift;
            w.end = w.end + shift;
        }
    return out;
}

Scene build_scene(const Plan& input) {
    const Plan plan = normalize_origin(input);
    Scene scene;
    scene.units = plan.displayUnits;
    scene.northAngleDeg = plan.orientation.northAngleDeg;
    if (plan.orientation.lat && plan.orientation.lon) scene.location = Location{*plan.orientation.lat, *plan.orientation.lon};
    else scene.warnings.push_back("no latitude/longitude; SketchUp geo-location and sun position are not set");

    scene.tags = {{tags::Walls, true},      {tags::Floors, true},     {tags::Ceilings, false},
                  {tags::Doors, true},      {tags::Windows, true},    {tags::DoorSwings, true},
                  {tags::Dimensions, true}};
    scene.materials = {{"Glass", 170, 200, 230, 0.35}};

    std::vector<OpeningStyle> styles;
    for (const Level& level : plan.levels)
        for (const Opening& o : level.openings) styles.push_back(opening_style(o, *level.find_wall(o.wallId)));
    const auto styleNames = name_styles(styles, plan.displayUnits);
    for (const auto& [style, name] : styleNames) scene.definitions.push_back(build_definition(style, name));

    Group house;
    house.name = "House";
    for (size_t i = 0; i < plan.levels.size(); ++i)
        house.groups.push_back(build_level(plan.levels[i], static_cast<int>(i), plan, styleNames, scene.warnings));
    scene.groups.push_back(std::move(house));
    return scene;
}

}  // namespace s2s

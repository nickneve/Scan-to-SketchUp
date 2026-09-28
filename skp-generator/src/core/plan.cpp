#include "plan.hpp"

#include <fstream>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

namespace s2s {

using nlohmann::json;

const Wall* Level::find_wall(const std::string& wallId) const {
    for (const Wall& w : walls)
        if (w.id == wallId) return &w;
    return nullptr;
}

const Room* Level::find_room(const std::string& roomId) const {
    for (const Room& r : rooms)
        if (r.id == roomId) return &r;
    return nullptr;
}

namespace {

template <typename T>
std::optional<T> optional_field(const json& j, const char* key) {
    if (auto it = j.find(key); it != j.end() && !it->is_null()) return it->get<T>();
    return std::nullopt;
}

Vec2 point(const json& j) { return {j.at("x").get<double>(), j.at("y").get<double>()}; }

Wall parse_wall(const json& j) {
    Wall w;
    w.id = j.at("id").get<std::string>();
    w.separator = j.value("kind", std::string("solid")) == "separator";
    w.start = point(j.at("start"));
    w.end = point(j.at("end"));
    w.thickness = j.at("thickness").get<double>();
    w.height = optional_field<double>(j, "height");
    w.isExterior = j.at("isExterior").get<bool>();
    w.confidence = optional_field<double>(j, "confidence");
    return w;
}

Opening parse_opening(const json& j) {
    Opening o;
    o.id = j.at("id").get<std::string>();
    o.wallId = j.at("wallId").get<std::string>();
    const std::string type = j.at("type").get<std::string>();
    if (type == "door") o.type = OpeningType::Door;
    else if (type == "window") o.type = OpeningType::Window;
    else if (type == "opening") o.type = OpeningType::Opening;
    else throw PlanError("opening " + o.id + ": unknown type '" + type + "'");
    o.offsetFromStart = j.at("offsetFromStart").get<double>();
    o.width = j.at("width").get<double>();
    o.height = j.at("height").get<double>();
    o.sillHeight = j.at("sillHeight").get<double>();
    o.operation = j.value("operation", std::string());
    if (auto it = j.find("swing"); it != j.end()) {
        Swing s;
        s.hingeAtStart = it->at("hingeEnd").get<std::string>() == "start";
        s.opensLeft = it->at("openSide").get<std::string>() == "left";
        o.swing = s;
    }
    o.confidence = optional_field<double>(j, "confidence");
    return o;
}

Room parse_room(const json& j) {
    Room r;
    r.id = j.at("id").get<std::string>();
    r.name = j.at("name").get<std::string>();
    r.type = j.at("type").get<std::string>();
    r.boundaryWallIds = j.at("boundaryWallIds").get<std::vector<std::string>>();
    r.ceilingHeight = optional_field<double>(j, "ceilingHeight");
    return r;
}

void check_level(const Level& level) {
    const std::string where = "level " + level.id + ": ";
    std::set<std::string> ids;
    auto unique = [&](const std::string& id) {
        if (!ids.insert(id).second) throw PlanError(where + "duplicate id " + id);
    };
    for (const Wall& w : level.walls) {
        unique(w.id);
        if (w.length() < 1.0) throw PlanError(where + "wall " + w.id + " has zero length");
        if (!w.separator && w.thickness <= 0) throw PlanError(where + "wall " + w.id + " needs a thickness");
    }
    for (const Opening& o : level.openings) {
        unique(o.id);
        const Wall* w = level.find_wall(o.wallId);
        if (!w) throw PlanError(where + "opening " + o.id + " references missing wall " + o.wallId);
        if (w->separator) throw PlanError(where + "opening " + o.id + " is on separator " + w->id);
        if (o.width <= 0 || o.height <= 0) throw PlanError(where + "opening " + o.id + " needs a size");
    }
    for (const Room& r : level.rooms) {
        unique(r.id);
        if (r.boundaryWallIds.size() < 3) throw PlanError(where + "room " + r.id + " needs at least 3 walls");
        for (const std::string& wid : r.boundaryWallIds)
            if (!level.find_wall(wid)) throw PlanError(where + "room " + r.id + " references missing wall " + wid);
    }
}

}  // namespace

Plan parse_plan(const std::string& jsonText) {
    json doc;
    try {
        doc = json::parse(jsonText);
    } catch (const json::parse_error& e) {
        throw PlanError(std::string("invalid JSON: ") + e.what());
    }

    try {
        Plan plan;
        plan.schemaVersion = doc.at("schemaVersion").get<std::string>();
        if (plan.schemaVersion != "0.1")
            throw PlanError("unsupported schemaVersion " + plan.schemaVersion + " (expected 0.1)");
        if (doc.at("units").get<std::string>() != "mm") throw PlanError("units must be mm");
        plan.displayUnits = doc.value("displayUnits", std::string("imperial")) == "metric" ? DisplayUnits::Metric
                                                                                            : DisplayUnits::Imperial;
        const json& o = doc.at("orientation");
        plan.orientation.northAngleDeg = o.at("northAngleDeg").get<double>();
        plan.orientation.headingSource = o.value("headingSource", std::string());
        plan.orientation.address = o.value("address", std::string());
        plan.orientation.lat = optional_field<double>(o, "lat");
        plan.orientation.lon = optional_field<double>(o, "lon");

        for (const json& lj : doc.at("levels")) {
            Level level;
            level.id = lj.at("id").get<std::string>();
            level.name = lj.at("name").get<std::string>();
            level.elevation = lj.at("elevation").get<double>();
            level.floorToCeiling = lj.at("floorToCeiling").get<double>();
            for (const json& w : lj.at("walls")) level.walls.push_back(parse_wall(w));
            for (const json& op : lj.at("openings")) level.openings.push_back(parse_opening(op));
            for (const json& r : lj.at("rooms")) level.rooms.push_back(parse_room(r));
            check_level(level);
            plan.levels.push_back(std::move(level));
        }
        if (plan.levels.empty()) throw PlanError("plan has no levels");
        return plan;
    } catch (const json::exception& e) {
        throw PlanError(std::string("malformed HousePlan: ") + e.what());
    }
}

Plan load_plan(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw PlanError("cannot open " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return parse_plan(ss.str());
}

}  // namespace s2s

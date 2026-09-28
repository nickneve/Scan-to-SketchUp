// In-memory HousePlan (schema 0.1). See schema/houseplan.schema.json for field meanings.
#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "math.hpp"

namespace s2s {

struct PlanError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

enum class DisplayUnits { Imperial, Metric };

struct Wall {
    std::string id;
    bool separator = false;  // invisible open-plan boundary; never generates geometry
    Vec2 start, end;         // centerline
    double thickness = 0;
    std::optional<double> height;
    bool isExterior = false;
    std::optional<double> confidence;

    double length() const { return distance(start, end); }
    Vec2 direction() const { return normalized(end - start); }
};

enum class OpeningType { Door, Window, Opening };

struct Swing {
    bool hingeAtStart = true;  // hinges on the opening edge nearer the wall's start
    bool opensLeft = true;     // leaf swings into the wall's left side (walking start -> end)
};

struct Opening {
    std::string id;
    std::string wallId;
    OpeningType type = OpeningType::Door;
    double offsetFromStart = 0;  // wall start -> near edge, along the centerline
    double width = 0, height = 0, sillHeight = 0;
    std::string operation;  // empty when not given
    std::optional<Swing> swing;
    std::optional<double> confidence;
};

struct Room {
    std::string id, name, type;
    std::vector<std::string> boundaryWallIds;
    std::optional<double> ceilingHeight;
};

struct Level {
    std::string id, name;
    double elevation = 0, floorToCeiling = 0;
    std::vector<Wall> walls;
    std::vector<Opening> openings;
    std::vector<Room> rooms;

    const Wall* find_wall(const std::string& wallId) const;
    const Room* find_room(const std::string& roomId) const;
};

struct Orientation {
    double northAngleDeg = 0;  // clockwise from plan +y to true north
    std::string headingSource;
    std::string address;
    std::optional<double> lat, lon;
};

struct Plan {
    std::string schemaVersion;
    DisplayUnits displayUnits = DisplayUnits::Imperial;
    Orientation orientation;
    std::vector<Level> levels;
};

// Parses a HousePlan document. Throws PlanError on malformed input or broken references.
// Full validation lives in schema/validate.py; this checks what the generator relies on.
Plan parse_plan(const std::string& jsonText);
Plan load_plan(const std::string& path);

}  // namespace s2s

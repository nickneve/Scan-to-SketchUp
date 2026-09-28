// SketchUp-shaped scene: exactly what the .skp writer will create, with no SDK dependency.
// Geometry is in millimeters in the coordinate system of the containing group
// (groups use the identity transform, so group geometry is in model coordinates;
// component definitions are in their own local coordinates).
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "math.hpp"
#include "plan.hpp"

namespace s2s {

// A planar face. loops[0] is the outer loop; any others are holes. The outer loop is
// counter-clockwise when viewed from the side the face normal points to.
struct Face {
    std::vector<std::vector<Vec3>> loops;
    std::string material;  // empty = default
};

// A circular arc, created as a SketchUp arc curve.
struct Arc {
    Vec3 center;
    Vec3 xaxis;   // unit vector toward angle 0
    Vec3 normal;  // arc runs counter-clockwise around this
    double radius = 0;
    double startAngle = 0, endAngle = 0;  // radians
    int segments = 12;
};

// Linear dimension between two points. The dimension line sits at start/end + offset.
struct Dimension {
    Vec3 start, end;
    Vec3 offset;
};

struct Instance {
    std::string definition;
    std::string name;
    std::string tag;
    Transform transform;
    std::map<std::string, std::string> attributes;  // "HousePlan" dictionary
};

struct Group {
    std::string name;
    std::string tag;  // empty = Untagged
    std::vector<Face> faces;
    std::vector<std::vector<Vec3>> edges;  // loose polylines (edges only)
    std::vector<Arc> arcs;
    std::vector<Group> groups;
    std::vector<Instance> instances;
    std::vector<Dimension> dimensions;
    std::map<std::string, std::string> attributes;  // "HousePlan" dictionary

    const Group* find(const std::string& childName) const;
};

struct ComponentDefinition {
    std::string name;
    std::string description;
    Group contents;  // contents.name / contents.tag are unused
};

struct Tag {
    std::string name;
    bool visible = true;
};

struct Material {
    std::string name;
    int r = 255, g = 255, b = 255;
    double alpha = 1.0;
};

struct Location {
    double lat = 0, lon = 0;
};

struct Scene {
    DisplayUnits units = DisplayUnits::Imperial;
    std::vector<Tag> tags;
    std::vector<Material> materials;
    std::vector<ComponentDefinition> definitions;
    std::vector<Group> groups;  // top-level entities
    std::optional<Location> location;
    double northAngleDeg = 0;  // clockwise from model +y (green) to true north
    std::vector<std::string> warnings;

    const Tag* find_tag(const std::string& name) const;
    const ComponentDefinition* find_definition(const std::string& name) const;
};

}  // namespace s2s

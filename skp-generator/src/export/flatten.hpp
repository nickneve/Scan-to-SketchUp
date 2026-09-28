// Flattens a Scene into world-space triangles and polylines for preview writers.
#pragma once

#include <string>
#include <vector>

#include "mesh.hpp"
#include "scene.hpp"

namespace s2s {

struct FlatPart {
    std::string path;      // "House/Level 1 - Main Floor/Walls/Wall - Office North"
    std::string tag;       // effective tag (nearest tagged ancestor, instance or group)
    std::string material;  // face material, empty = default
    std::string kind;      // top-level category: Walls, Floors, Ceilings, Doors, Windows ...
    std::vector<Triangle> triangles;
    std::vector<std::vector<Vec3>> lines;
};

// One part per (group or instance sub-group, material). Parts on hidden tags are skipped
// unless includeHidden is set.
std::vector<FlatPart> flatten(const Scene& scene, bool includeHidden);

std::vector<Vec3> sample_arc(const Arc& arc);

}  // namespace s2s

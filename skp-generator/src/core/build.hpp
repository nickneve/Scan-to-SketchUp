// HousePlan -> Scene: everything the .skp writer needs, following docs/PROJECT_BRIEF.md
// "SketchUp modeling conventions".
#pragma once

#include "plan.hpp"
#include "scene.hpp"

namespace s2s {

// Tag names used in generated models.
namespace tags {
inline constexpr const char* Walls = "Walls";
inline constexpr const char* Floors = "Floors";
inline constexpr const char* Ceilings = "Ceilings";
inline constexpr const char* Doors = "Doors";
inline constexpr const char* Windows = "Windows";
inline constexpr const char* DoorSwings = "Door Swings";
inline constexpr const char* Dimensions = "Dimensions";
}  // namespace tags

// Throws PlanError when the plan can't be built (rooms that don't close, openings that don't fit...).
Scene build_scene(const Plan& plan);

// The plan shifted so the house's front-left exterior corner (minimum x and y of all wall
// outlines) sits at the origin.
Plan normalize_origin(const Plan& plan);

}  // namespace s2s

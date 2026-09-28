// Preview and report writers. None of these are the product output (that's .skp); they let
// the owner and the tests inspect what the generator will build.
#pragma once

#include <string>

#include "plan.hpp"
#include "scene.hpp"

namespace s2s {

// Wavefront OBJ + MTL (meters, Y-up, as most viewers expect). Writes <path> and <path minus .obj>.mtl.
void write_obj(const Scene& scene, const std::string& path, bool includeHidden = false);

// Blueprint-style floor plan of one level as SVG: walls cut at 4 ft, doors with swings,
// windows, room names and areas, all dimensions.
std::string plan_svg(const Plan& plan, const Scene& scene, size_t levelIndex);

// Self-contained HTML page with an orbitable 3D model and per-tag visibility toggles.
std::string viewer_html(const Scene& scene, const std::string& title);

// Human-readable outline of the scene as SketchUp's Outliner would show it, plus tags,
// component definitions, solid checks and warnings.
std::string scene_report(const Scene& scene);

}  // namespace s2s

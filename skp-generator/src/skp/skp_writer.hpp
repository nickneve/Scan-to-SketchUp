// Scene -> .skp via the SketchUp C SDK. Built only when S2S_SKETCHUP_SDK_DIR is set.
#pragma once

#include <string>

#include "scene.hpp"

namespace s2s {

// Writes the scene as a SketchUp model. Throws std::runtime_error on SDK errors.
void write_skp(const Scene& scene, const std::string& path);

}  // namespace s2s

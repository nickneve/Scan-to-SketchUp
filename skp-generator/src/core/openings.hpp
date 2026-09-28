// Door, window and cased-opening components.
//
// Component local axes: x along the opening's width (0..width), y through the wall from its
// right face (0) to its left face (thickness), z up from the sill. Hinged doors are modelled
// with hinges at x = 0 swinging toward +y; instances are mirrored for other hands.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "plan.hpp"
#include "scene.hpp"

namespace s2s {

struct OpeningStyle {
    OpeningType type;
    std::string variant;  // door: hinged | double | sliding | pocket | bifold; others: ""
    double width, height, thickness;

    std::string base_name(DisplayUnits units) const;  // "Door 32x80", "Window 36x48", ...
    bool operator<(const OpeningStyle& o) const;
};

OpeningStyle opening_style(const Opening& opening, const Wall& wall);

// Assigns unique definition names. Styles sharing a base name get the wall thickness
// appended ("Door 32x80 (6.5in wall)"), then a counter if still ambiguous.
std::map<OpeningStyle, std::string> name_styles(const std::vector<OpeningStyle>& styles, DisplayUnits units);

ComponentDefinition build_definition(const OpeningStyle& style, const std::string& name);

// Placement of an opening's component instance in model coordinates.
Transform opening_transform(const Opening& opening, const Wall& wall, double levelElevation);

}  // namespace s2s

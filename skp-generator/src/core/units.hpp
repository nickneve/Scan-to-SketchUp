// Length formatting for names, labels and previews.
#pragma once

#include <string>

#include "math.hpp"
#include "plan.hpp"

namespace s2s {

// Architectural feet-inches to the nearest 1/16" (12'-0", 11'-7 13/16", 7 1/2"), or "3658 mm".
std::string format_length(double mm, DisplayUnits units);

// "120 sq ft" or "11.2 m²".
std::string format_area(double mm2, DisplayUnits units);

// Whole inches (for component names such as "Door 32x80") or whole millimeters.
long long nominal_size(double mm, DisplayUnits units);

// Compact thickness for disambiguating names: "4.5in" or "114mm".
std::string format_thickness(double mm, DisplayUnits units);

// 8-point compass name ("North", "Northeast", ...) of a plan direction, given the plan's
// north angle (clockwise from +y to true north, degrees).
std::string compass_name(Vec2 direction, double northAngleDeg);

}  // namespace s2s

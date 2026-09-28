#include <doctest/doctest.h>

#include "units.hpp"

using namespace s2s;

TEST_CASE("architectural lengths") {
    CHECK(format_length(3657.6, DisplayUnits::Imperial) == "12'-0\"");
    CHECK(format_length(3658, DisplayUnits::Imperial) == "12'-0\"");
    CHECK(format_length(3552, DisplayUnits::Imperial) == "11'-7 13/16\"");
    CHECK(format_length(190.5, DisplayUnits::Imperial) == "7 1/2\"");
    CHECK(format_length(304.8 * 31 + 25.4, DisplayUnits::Imperial) == "31'-1\"");
    CHECK(format_length(3552.4, DisplayUnits::Metric) == "3552 mm");
}

TEST_CASE("nominal sizes for component names") {
    CHECK(nominal_size(914, DisplayUnits::Imperial) == 36);
    CHECK(nominal_size(813, DisplayUnits::Imperial) == 32);
    CHECK(nominal_size(2032, DisplayUnits::Imperial) == 80);
    CHECK(nominal_size(813, DisplayUnits::Metric) == 813);
}

TEST_CASE("wall thickness labels") {
    CHECK(format_thickness(114, DisplayUnits::Imperial) == "4.5in");
    CHECK(format_thickness(165, DisplayUnits::Imperial) == "6.5in");
    CHECK(format_thickness(152.4, DisplayUnits::Imperial) == "6in");
    CHECK(format_thickness(114, DisplayUnits::Metric) == "114mm");
}

TEST_CASE("compass directions follow the north angle") {
    CHECK(compass_name({0, 1}, 0) == "North");
    CHECK(compass_name({1, 0}, 0) == "East");
    CHECK(compass_name({0, -1}, 0) == "South");
    CHECK(compass_name({-1, 1}, 0) == "Northwest");
    CHECK(compass_name({0, -1}, 17.5) == "South");
    CHECK(compass_name({1, 0}, 17.5) == "East");
    // North angle 90: true north points along +x.
    CHECK(compass_name({1, 0}, 90) == "North");
    CHECK(compass_name({0, 1}, 90) == "West");
}

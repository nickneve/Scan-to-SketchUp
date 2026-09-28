#include "units.hpp"

#include <cmath>
#include <cstdio>
#include <numeric>

namespace s2s {

namespace {

constexpr double kMmPerInch = 25.4;

std::string fmt(const char* f, double v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, f, v);
    return buf;
}

}  // namespace

std::string format_length(double mm, DisplayUnits units) {
    if (units == DisplayUnits::Metric) return std::to_string(std::llround(mm)) + " mm";

    const bool negative = mm < 0;
    long long sixteenths = std::llround(std::abs(mm) / kMmPerInch * 16);
    const long long feet = sixteenths / (12 * 16);
    sixteenths -= feet * 12 * 16;
    const long long inches = sixteenths / 16;
    long long frac = sixteenths % 16, denom = 16;
    if (frac) {
        long long g = std::gcd(frac, denom);
        frac /= g;
        denom /= g;
    }

    std::string in = std::to_string(inches);
    if (frac) in += " " + std::to_string(frac) + "/" + std::to_string(denom);
    std::string out = feet ? std::to_string(feet) + "'-" + in + "\"" : in + "\"";
    return negative ? "-" + out : out;
}

std::string format_area(double mm2, DisplayUnits units) {
    if (units == DisplayUnits::Metric) return fmt("%.1f m\xC2\xB2", mm2 / 1e6);
    return std::to_string(std::llround(mm2 / (kMmPerInch * kMmPerInch * 144))) + " sq ft";
}

long long nominal_size(double mm, DisplayUnits units) {
    return std::llround(units == DisplayUnits::Metric ? mm : mm / kMmPerInch);
}

std::string format_thickness(double mm, DisplayUnits units) {
    if (units == DisplayUnits::Metric) return std::to_string(std::llround(mm)) + "mm";
    std::string s = fmt("%.1f", mm / kMmPerInch);
    if (s.size() > 2 && s.substr(s.size() - 2) == ".0") s.resize(s.size() - 2);
    return s + "in";
}

std::string compass_name(Vec2 direction, double northAngleDeg) {
    const double a = northAngleDeg * kPi / 180;
    const Vec2 north{std::sin(a), std::cos(a)};
    const Vec2 east{std::cos(a), -std::sin(a)};
    double bearing = std::atan2(dot(direction, east), dot(direction, north)) * 180 / kPi;
    if (bearing < 0) bearing += 360;
    static const char* names[] = {"North", "Northeast", "East", "Southeast",
                                  "South", "Southwest", "West", "Northwest"};
    return names[static_cast<int>(std::floor((bearing + 22.5) / 45)) % 8];
}

}  // namespace s2s

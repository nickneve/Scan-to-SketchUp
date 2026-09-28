// Small vector and transform types. All plan and scene coordinates are millimeters.
#pragma once

#include <cmath>

namespace s2s {

constexpr double kPi = 3.14159265358979323846;

struct Vec2 {
    double x = 0, y = 0;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator-(Vec2 a) { return {-a.x, -a.y}; }
inline Vec2 operator*(Vec2 a, double s) { return {a.x * s, a.y * s}; }
inline Vec2 operator*(double s, Vec2 a) { return a * s; }
inline double dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline double cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline double length(Vec2 a) { return std::hypot(a.x, a.y); }
inline double distance(Vec2 a, Vec2 b) { return length(a - b); }
inline Vec2 normalized(Vec2 a) {
    double l = length(a);
    return l > 0 ? a * (1.0 / l) : Vec2{};
}
// Unit normals of a direction, as seen walking along it.
inline Vec2 left_of(Vec2 d) { return {-d.y, d.x}; }
inline Vec2 right_of(Vec2 d) { return {d.y, -d.x}; }

struct Vec3 {
    double x = 0, y = 0, z = 0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator*(double s, Vec3 a) { return a * s; }
inline double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline double distance(Vec3 a, Vec3 b) { return length(a - b); }
inline Vec3 normalized(Vec3 a) {
    double l = length(a);
    return l > 0 ? a * (1.0 / l) : Vec3{};
}

inline Vec3 to3(Vec2 p, double z = 0) { return {p.x, p.y, z}; }
inline Vec2 to2(Vec3 p) { return {p.x, p.y}; }

// Affine transform given by an origin and three axes (columns). Axes may be mirrored
// (negative determinant), which SketchUp supports for component instances.
struct Transform {
    Vec3 origin{};
    Vec3 xaxis{1, 0, 0};
    Vec3 yaxis{0, 1, 0};
    Vec3 zaxis{0, 0, 1};

    Vec3 apply(Vec3 p) const { return origin + xaxis * p.x + yaxis * p.y + zaxis * p.z; }
    double determinant() const { return dot(xaxis, cross(yaxis, zaxis)); }
};

}  // namespace s2s

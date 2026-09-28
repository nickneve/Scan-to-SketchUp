#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "exporters.hpp"
#include "helpers.hpp"

using namespace s2s;

TEST_CASE("report lists the outliner, definitions and no broken solids") {
    const Scene scene = build_scene(test::fixture("03-three-rooms-hallway"));
    const std::string r = scene_report(scene);
    CHECK(r.find("Wall - Bedroom / Hallway") != std::string::npos);
    CHECK(r.find("<Door 32x80>") != std::string::npos);
    CHECK(r.find("Ceilings  (hidden)") != std::string::npos);
    CHECK(r.find("NOT SOLID") == std::string::npos);
}

TEST_CASE("floor plan SVG shows rooms, walls and dimensions") {
    const Plan plan = test::fixture("03-three-rooms-hallway");
    const std::string svg = plan_svg(plan, build_scene(plan), 0);
    CHECK(svg.rfind("<svg", 0) == 0);
    CHECK(svg.find("Living Room") != std::string::npos);
    CHECK(svg.find("31'-1&quot;") != std::string::npos);  // overall front (quotes are escaped)
    CHECK(svg.find("<polyline") != std::string::npos);  // door swings
}

TEST_CASE("3D viewer embeds the scene") {
    const std::string html = viewer_html(build_scene(test::fixture("01-rectangular-room")), "test");
    CHECK(html.find("Wall - Office North") != std::string::npos);
    CHECK(html.find("__DATA__") == std::string::npos);
}

TEST_CASE("OBJ export writes geometry and materials") {
    const auto dir = std::filesystem::temp_directory_path() / "s2s_test_obj";
    std::filesystem::create_directories(dir);
    const auto path = (dir / "room.obj").string();
    write_obj(build_scene(test::fixture("01-rectangular-room")), path);
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string obj = ss.str();
    CHECK(obj.find("mtllib room.mtl") != std::string::npos);
    CHECK(obj.find("g House/Level_1_-_Main_Floor/Walls/Wall_-_Office_North") != std::string::npos);
    CHECK(obj.find("usemtl Glass") != std::string::npos);
    CHECK(obj.find("Ceiling") == std::string::npos);  // hidden tag left out by default
    CHECK(std::filesystem::exists(dir / "room.mtl"));
}

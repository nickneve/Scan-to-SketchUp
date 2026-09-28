cmake_minimum_required(VERSION 3.20)
project(skp_generator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(S2S_BUILD_TESTS "Build unit tests" ON)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /EHsc)
else()
  add_compile_options(-Wall -Wextra -Wno-missing-field-initializers)
endif()

# Geometry core: HousePlan -> Scene. No SketchUp SDK dependency.
add_library(s2s_core STATIC
  src/core/build.cpp
  src/core/dimensions.cpp
  src/core/mesh.cpp
  src/core/openings.cpp
  src/core/plan.cpp
  src/core/polygon.cpp
  src/core/rooms.cpp
  src/core/scene.cpp
  src/core/units.cpp
  src/core/walls.cpp
)
target_include_directories(s2s_core PUBLIC src/core)
target_include_directories(s2s_core SYSTEM PRIVATE third_party)

# Preview and report writers (OBJ, SVG plan, HTML 3D viewer, text report).
add_library(s2s_export STATIC
  src/export/flatten.cpp
  src/export/html.cpp
  src/export/obj.cpp
  src/export/report.cpp
  src/export/svg.cpp
)
target_include_directories(s2s_export PUBLIC src/export)
target_include_directories(s2s_export SYSTEM PRIVATE third_party)
target_link_libraries(s2s_export PUBLIC s2s_core)

add_executable(housegen src/cli/main.cpp)
target_link_libraries(housegen PRIVATE s2s_export)

if(S2S_BUILD_TESTS)
  enable_testing()
  add_executable(s2s_tests
    tests/main.cpp
    tests/test_polygon.cpp
    tests/test_units.cpp
    tests/test_walls.cpp
    tests/test_rooms.cpp
    tests/test_scene.cpp
    tests/test_exports.cpp
  )
  target_include_directories(s2s_tests SYSTEM PRIVATE third_party)
  target_link_libraries(s2s_tests PRIVATE s2s_export)
  target_compile_definitions(s2s_tests PRIVATE S2S_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../fixtures")
  add_test(NAME s2s_tests COMMAND s2s_tests)
endif()

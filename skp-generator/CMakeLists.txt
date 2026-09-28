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

# .skp output needs the SketchUp C SDK (licensed separately; never committed). Point
# S2S_SKETCHUP_SDK_DIR at the unzipped SDK to enable `housegen --skp`.
set(S2S_SKETCHUP_SDK_DIR "" CACHE PATH "Unzipped SketchUp C SDK (enables .skp output)")
if(S2S_SKETCHUP_SDK_DIR)
  find_path(SKETCHUP_INCLUDE_DIR SketchUpAPI/sketchup.h
    PATHS "${S2S_SKETCHUP_SDK_DIR}" PATH_SUFFIXES headers include NO_DEFAULT_PATH REQUIRED)
  file(GLOB_RECURSE SKETCHUP_LIBS "${S2S_SKETCHUP_SDK_DIR}/*SketchUpAPI.lib" "${S2S_SKETCHUP_SDK_DIR}/*libSketchUpAPI.so"
    "${S2S_SKETCHUP_SDK_DIR}/*SketchUpAPI.framework")
  list(FILTER SKETCHUP_LIBS INCLUDE REGEX "x64|SketchUpAPI.framework|\.so$")
  if(NOT SKETCHUP_LIBS)
    message(FATAL_ERROR "SketchUpAPI library not found under ${S2S_SKETCHUP_SDK_DIR}")
  endif()
  list(GET SKETCHUP_LIBS 0 SKETCHUP_LIB)
  message(STATUS "SketchUp SDK: ${SKETCHUP_INCLUDE_DIR}, ${SKETCHUP_LIB}")

  add_library(s2s_skp STATIC src/skp/skp_writer.cpp)
  target_include_directories(s2s_skp PUBLIC src/skp)
  target_include_directories(s2s_skp SYSTEM PRIVATE "${SKETCHUP_INCLUDE_DIR}")
  target_link_libraries(s2s_skp PUBLIC s2s_core "${SKETCHUP_LIB}")
  target_link_libraries(housegen PRIVATE s2s_skp)
  target_compile_definitions(housegen PRIVATE S2S_HAVE_SKP=1)

  if(WIN32)  # the SDK DLLs must sit next to housegen.exe
    get_filename_component(SKETCHUP_BIN_DIR "${SKETCHUP_LIB}" DIRECTORY)
    file(GLOB SKETCHUP_DLLS "${SKETCHUP_BIN_DIR}/*.dll")
    foreach(dll ${SKETCHUP_DLLS})
      add_custom_command(TARGET housegen POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:housegen>")
    endforeach()
  endif()
endif()

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

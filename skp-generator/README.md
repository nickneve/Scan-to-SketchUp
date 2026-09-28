# skp-generator

Command-line tool that turns a HousePlan JSON file into a SketchUp model, plus previews
for checking the result without SketchUp.

```
housegen PLAN.json [--out DIR] [--obj] [--svg] [--html] [--report] [--skp]
```

With no format flags it writes every preview:

| Output | File | What it's for |
| --- | --- | --- |
| Floor plan | `<plan>-plan-L1.svg` | Blueprint-style plan: walls cut at 4 ft, door swings, windows, room names and areas, every dimension |
| 3D viewer | `<plan>-3d.html` | Orbit the model in a browser, toggle tags like SketchUp (needs internet for three.js) |
| Report | `<plan>-report.txt` | The Outliner tree SketchUp will show, tags, component definitions, solid checks, warnings |
| OBJ | `<plan>.obj` + `.mtl` | Generic 3D file (meters, Y-up) |
| SketchUp | `<plan>.skp` | Needs a build with the SketchUp C SDK (see below) |

## Build (Windows)

Needs Visual Studio 2026 with C++ (it bundles CMake and Ninja).

```
build.cmd          # configure + build (Release) into build\
build.cmd test     # ...and run the unit tests
build\housegen.exe ..\fixtures\plans\03-three-rooms-hallway.json --out build\preview
```

On Linux/macOS: `cmake -S . -B build && cmake --build build && ctest --test-dir build`.

## Enabling .skp output

1. Download the SketchUp C SDK from developer.sketchup.com and unzip it somewhere outside
   the repo (or under any `sketchup-sdk/` folder, which git ignores). Never commit it.
2. Configure with the SDK path, then build:
   ```
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DS2S_SKETCHUP_SDK_DIR=C:\path\to\sdk
   cmake --build build
   ```
   The SDK DLLs are copied next to `housegen.exe`.
3. `housegen PLAN.json --skp` writes `PLAN.skp`; open it in SketchUp (Web, Free, Pro, iPad).

`src/skp/skp_writer.cpp` was written from the API reference before SDK access, so the first
build may need small fixes. Lines marked `VERIFY` need checking in a real model (arc
direction, dimension offset side, north angle direction).

## How it's organised

```
src/core/    HousePlan -> Scene. No SDK dependency; everything testable here.
  plan       JSON loading and reference checks
  walls      footprints with corner joins, wall solids with openings cut through
  rooms      room outlines on inside faces (floors, ceilings)
  openings   door/window/opening components and their placement
  dimensions blueprint dimension set
  build      assembles the Scene: hierarchy, names, tags, materials
  scene      the SketchUp-shaped intermediate the .skp writer consumes
src/export/  previews (OBJ, SVG, HTML, report)
src/skp/     Scene -> .skp via the SketchUp C SDK (built only with the SDK)
src/cli/     housegen
tests/       doctest suite over the fixtures in ../fixtures/plans
```

The Scene mirrors what SketchUp will contain (groups, component definitions and instances,
tags, materials, dimensions, geo-location), so the tests check the modeling conventions
without SketchUp, and the .skp writer is a thin translation.

### Geometry rules

- Walls are centerlines + thickness. Corners are mitred; at a T the two collinear walls run
  through and the third stops at their face; free ends are square.
- Each wall is one closed solid with openings cut straight through (checked in tests).
- Floors and ceilings follow the rooms' inside faces; open-plan separators run along their
  centerline.
- Doors and windows are components named by nominal size (`Door 32x80`). Identical sizes
  share a definition; same size in a different wall thickness gets `(4.5in wall)` appended.
  Hinged doors are one definition mirrored per hand; the swing arc is a nested group on the
  `Door Swings` tag.
- The origin moves to the front-left exterior corner.

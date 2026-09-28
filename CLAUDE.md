# Scan-to-SketchUp

iOS app that turns a LiDAR scan (or import) of a home into a clean, professionally structured SketchUp model. Full brief and decisions log: [docs/PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md).

## Current phase

**Phase 0 · Foundations** — gate passed 2026-09-27 (CI green on GitHub). Built: brief, HousePlan JSON Schema v0.1, 3 fixture plans, validator + 13 invalid fixtures, CI workflow.
Deferred to the Mac: Swift `Codable` types, Mac/Xcode check.

**Phase 1 · `.skp` generator** — in progress. SketchUp C SDK access requested 2026-09-27, awaiting reply.
- Done (2026-09-28): SDK-independent geometry core in `skp-generator/` (HousePlan → Scene), previews (SVG plan, HTML 3D viewer, OBJ, report), `housegen` CLI, 38 doctest tests, CI on Linux + Windows.
- Drafted, uncompiled: `src/skp/skp_writer.cpp` (Scene → .skp). Built only with `-DS2S_SKETCHUP_SDK_DIR`. Fix compile errors and check the `VERIFY` items on first SDK build.
- Remaining for the gate: SDK build, owner's house entered as a HousePlan, manual checklist `docs/phase1-manual-checklist.md`.

Repo: https://github.com/nickneve/Scan-to-SketchUp (public — never commit SDK files).

## Windows toolchain

Visual Studio Community 2026 (MSVC 19.51) with its bundled CMake and Ninja; neither is on PATH. Build from a shell initialized by
`"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64`.
Keep build directories on short paths: CMake warns when object paths exceed 250 characters.

Work one phase at a time; stop at each gate for the owner to verify on their own house.

## Architecture

Every input converts to one HousePlan JSON file; every output reads from it. Converters, the editor and generators never talk to each other directly, and the iOS app never emits SketchUp geometry.

- `/schema` — `houseplan.schema.json` (v0.1), `validate.py` (schema + geometry rules), `regional-defaults.json`
- `/fixtures/plans` — valid plans every component is tested against; `/fixtures/invalid` — plans the validator must reject
- `/skp-generator` — (Phase 1) C/C++ command-line tool: HousePlan → `.skp` via SketchUp C SDK
- `/ios` — (Phase 2+) Swift/SwiftUI, iOS 17+, RoomPlan
- `/docs` — brief and design notes

Output is `.skp` via the C SDK, **not** a Ruby extension: the owner uses SketchUp Web (Free), which can't run extensions. Ruby extension is a "Later" item for Pro users.

## HousePlan conventions

- All lengths in **mm**; display imperial by default, metric optional (`displayUnits`).
- Axes match SketchUp: origin at front-left exterior corner, +x (red) along the front wall, +y (green) toward the back, z up.
- Walls are **centerlines** + thickness; "left"/"right" face = as seen walking start → end. Walls meet only at endpoints (split at T-junctions).
- `kind: "separator"` walls are invisible open-plan room boundaries (thickness 0, no geometry, no openings).
- Openings: `offsetFromStart` = wall start → near edge of opening, along the centerline.
- Rooms list `boundaryWallIds` in loop order. `Room.ceilingHeight` overrides `Level.floorToCeiling`.
- `measurements[]` is the audit trail; manual/laser values override scan values. Users measure **inside faces**; `face` records which.
- `northAngleDeg` = clockwise from +y to true north.
- Changing the schema: bump `schemaVersion`, update fixtures, keep old files loadable where possible.

## SketchUp output conventions (proposed; validate with a designer)

Hierarchy `House > Level 1 - Main Floor > Walls / Floors / Ceilings / Doors / Windows / Dimensions`, plus hidden `Scan Reference` when a mesh exists. Tags: Walls, Floors, Ceilings (off), Doors, Windows, Door Swings, Dimensions, Scan Reference (off). Raw geometry untagged; tags on groups/components only (each wall/floor/ceiling group and each door/window instance; the Dimensions container).
- Walls: one solid group each, named `Wall - <Room> <Compass>` (exterior) or `Wall - <Room A> / <Room B>` (interior, alphabetical); duplicates numbered.
- Doors/windows: components named by nominal size (`Door 32x80`, metric `Door 813x2032`), frame + leaf/glass as nested groups; swing arcs in a nested group on `Door Swings`; hinge/swing hands by mirroring one definition. Same size in another wall thickness → `(4.5in wall)` suffix.
- Dimensions (owner chose "standard + opening locations"): room inside runs (opposite equal sides once), interior-wall opening strings on the swing side, exterior strings per side: openings, segments, overall.
- Geo-location from lat/lon; north from `northAngleDeg`. Model units Architectural 1/16" (metric: mm).

**Ask the owner before changing anything designers see in SketchUp** (naming, grouping, tags, units).

## Commands

```
pip install -r schema/requirements.txt
python schema/validate.py fixtures/plans
python schema/validate.py --expect-invalid fixtures/invalid

cd skp-generator
build.cmd test                                   # build + unit tests (Windows)
build\housegen.exe ..ixtures\plans-three-rooms-hallway.json --out build\preview
```

## Rules

- Verify current Apple RoomPlan and SketchUp C SDK docs before relying on an API.
- Never commit the SketchUp SDK (any `sketchup-sdk/` folder is gitignored).
- MSVC 19.51 hit an internal compiler error on comma-expression assignments inside lambdas; write them as separate statements.
- Update this file at the end of each phase: what was built, schema changes, what's next.

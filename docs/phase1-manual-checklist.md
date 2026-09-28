# Phase 1 manual checklist

The automated tests check the model's structure. These checks need a person looking at the
result. Part A works today with the previews; Part B needs the SketchUp SDK build.

## Part A: previews (now)

Generate previews for a plan:

```
cd skp-generator
build\housegen.exe ..\fixtures\plans\03-three-rooms-hallway.json --out build\preview
```

Open `build\preview\*-plan-L1.svg` and `*-3d.html` in a browser.

- [ ] Floor plan reads like a blueprint: walls solid black, gaps at doors, window symbols.
- [ ] Each door swing opens into the room you expect, hinged on the side you expect.
- [ ] Room names and areas are right.
- [ ] Inside dimensions match what you'd measure with a tape (inside face to inside face).
- [ ] Outside strings: openings, then wall segments, then the overall length.
- [ ] Nothing important is buried under overlapping dimensions.
- [ ] In 3D, wall heights and window sills look right; ceilings hidden by default.
- [ ] Report (`*-report.txt`): names in the Outliner are what a designer would expect.

## Part B: in SketchUp (after the SDK arrives)

Open the `.skp` in SketchUp for Web.

Structure
- [ ] Outliner shows `House > Level 1 - Main Floor > Walls / Floors / Ceilings / Doors / Windows / Dimensions` and nothing loose at the top level.
- [ ] Wall names read like `Wall - Office North`; shared walls like `Wall - Bedroom / Hallway`.
- [ ] Tags panel: Walls, Floors, Ceilings (off), Doors, Windows, Door Swings, Dimensions.
- [ ] Hiding each tag hides exactly what it should.
- [ ] Entity Info on a wall says **Solid group**.

Doors and windows
- [ ] Components panel lists `Door 32x80`, `Window 36x48`, etc.; identical sizes share one.
- [ ] Door swings open the right way (mirrored doors look right, not inside-out).
- [ ] Door swing arcs curve the right way (VERIFY item in the writer).

Dimensions and units
- [ ] Model Info > Units is Architectural, 1/16".
- [ ] Dimensions sit on the side of the wall the SVG shows (VERIFY item: offset sign).
- [ ] Dimension values match the SVG.

Orientation
- [ ] Origin is the front-left exterior corner; the front wall runs along the red axis.
- [ ] With lat/lon in the plan: Model Info > Geo-location shows the location.
- [ ] Turn on View > Toolbars > Solar North, show north: it matches the SVG's north arrow
      (VERIFY item: north angle direction).
- [ ] Shadows at a known time of day fall where they do at the real house.

Faces
- [ ] Floors show their front (white/default) side from above, not the blue-gray back.
      (SketchUp sometimes flips faces drawn at ground level.)

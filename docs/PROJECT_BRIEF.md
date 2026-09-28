# Scan-to-SketchUp: Project Handoff for Claude Code

Sep 27, 2026 · @Nick

## Overview

Build one iOS app that takes an existing home from walkthrough (or blueprint) to a clean, professionally structured SketchUp model, with no second round of manual modeling. Today designers measure a house once on site, then re-enter everything by hand in SketchUp. This app collapses those two passes into one.

**Who it's for:** interior designers first; homeowners modeling their own house second. The owner's own house is the first test case.

**The one-tool vision:** capture, correct, and export all happen in this app. Users should never need a separate scanning app, a separate floor-plan app, and SketchUp cleanup work. Importing from other scanning apps is still supported, so users who already scanned elsewhere are not locked out.

**What makes it different:** existing scanners produce loose geometry. This app's value is the output: a SketchUp model with real wall thickness, doors and windows as components, rooms grouped, tags applied, and north set correctly, ready for design work.

**Core flow:** scan or import → review a 2D floor plan → correct measurements (manual or laser) → set orientation → generate the SketchUp model.

## Scope

v1 is a working path from a LiDAR scan of a single-story home to a clean SketchUp model. Everything else layers on top of that path.

| Area | v1 | Later | Out of scope |
| --- | --- | --- | --- |
| Capture | LiDAR multi-room scan (RoomPlan) | Photo/video layout for non-LiDAR phones | Android |
| Import | RoomPlan USDZ/JSON, generic OBJ/USDZ | DXF, IFC, other apps' native exports | Proprietary formats with no export |
| Blueprint | — | PDF/image plan interpretation with scale calibration | Hand-sketch interpretation |
| Editing | 2D plan view, edit wall lengths, move openings, add missing doors/windows | Bluetooth laser measure input | Full CAD editor |
| Output | Direct .skp via C SDK generator + JSON | Ruby extension for Pro users, Trimble cloud upload | Other CAD formats beyond basic OBJ/DAE |
| Building | Single story, straight walls | Multiple floors, stairs, angled and curved walls, roofs, exteriors (from photos plus measurements) | MEP (plumbing, electrical, HVAC) |
| Orientation | Compass heading at scan + address | — | — |
| Furniture | Ignored | Furniture boxes from scan, then furniture recognition | — |

## Architecture

Every input is converted into one canonical HousePlan file, and every output is generated from it. This keeps capture, import, and SketchUp generation independent, so each can be built and tested alone.

```
Inputs                         Core                          Outputs
LiDAR scan (RoomPlan)   ─┐     Orientation (heading+address)  ┌─> SketchUp .skp generator (C SDK, v1)*
Third-party import      ─┤              │                     ├─> SketchUp Ruby extension (later, Pro only)*
Blueprint (later)       ─┼──>  HousePlan JSON  <── corrections ┼─> OBJ / DAE export
Photo/video (later)     ─┘     (single source of truth)        └─> 2D floor plan PDF
                                        │           ▲
                                        └─> 2D plan editor (edit lengths, laser input)
```
\* Swapped from the original diagram on 2026-09-27; see Decisions log.

The 2D editor and the HousePlan loop: the user corrects the plan, and the model regenerates from the corrected file.

**iOS app.** Swift and SwiftUI, targeting iOS 17+ so RoomPlan's multi-room capture and merging are available. ARKit for scanning, CoreLocation for compass heading, local JSON files for storage. LiDAR requires an iPhone Pro or iPad Pro; non-LiDAR devices get import and manual entry only in v1.

**HousePlan.** A versioned JSON format with a JSON Schema file in the repo and matching Swift `Codable` types. All units stored in millimeters internally; display in feet and inches by default.

**SketchUp generation (v1, changed 2026-09-27).** A small desktop command-line tool (Windows now, macOS later) that reads a HousePlan and writes a `.skp` file using the SketchUp C SDK. The file opens in SketchUp Web, Free, Pro, Go and iPad, so no extension install is needed.

**SketchUp generation (later).** A Ruby extension (.rbz) for SketchUp Pro desktop users who prefer an "Import HousePlan" menu item, and/or running the generator on a server so the iOS app can produce `.skp` directly.

*Original plan (superseded):* v1 was a Ruby extension, with C SDK output later. SketchUp Web/Free cannot run extensions, so the order was swapped.

**Backend.** None in v1; everything runs on device. Add a small server later for blueprint interpretation (keeps the AI API key off the device) and for server-side .skp generation.

**Repo layout.** `/ios` (app), `/schema` (HousePlan JSON Schema + examples), `/skp-generator` (C SDK command-line tool; was `/sketchup-extension` Ruby), `/fixtures` (sample scans and plans for tests), `/docs`.

## Canonical HousePlan schema

The HousePlan is the contract between every part of the system, so design it first and change it deliberately. Below is a starting shape; Claude Code should formalize it as JSON Schema and refine it after testing real scans.

| Entity | Key fields | Notes |
| --- | --- | --- |
| HousePlan | schemaVersion, units, source, orientation, levels\[\] | Units stored in mm |
| Orientation | northAngleDeg, headingSource, address, lat, lon | northAngleDeg = rotation from plan +Y to true north |
| Level | id, name, elevation, floorToCeiling, rooms\[\], walls\[\] | One per floor |
| Wall | id, start{x,y}, end{x,y}, thickness, height, isExterior, confidence, measuredLength? | Walls are shared between rooms, not duplicated |
| Opening | id, wallId, type (door/window/opening), offsetFromStart, width, height, sillHeight, swing?, confidence | Positioned along its wall |
| Room | id, name, type, boundaryWallIds\[\], ceilingHeight | Name drives SketchUp group names |
| Measurement | targetId, field, value, source (scan/manual/laser) | Manual values always override scan values |

```json
{
  "schemaVersion": "0.1",
  "units": "mm",
  "source": { "type": "roomplan", "capturedAt": "2026-09-27T14:00:00Z" },
  "orientation": { "northAngleDeg": 17.5, "headingSource": "compass", "address": "123 Main St" },
  "levels": [{
    "id": "L1", "name": "Main Floor", "elevation": 0, "floorToCeiling": 2743,
    "walls": [{ "id": "W1", "start": {"x": 0, "y": 0}, "end": {"x": 4267, "y": 0},
                "thickness": 114, "height": 2743, "isExterior": true, "confidence": 0.92 }],
    "openings": [{ "id": "D1", "wallId": "W1", "type": "door", "offsetFromStart": 610,
                   "width": 813, "height": 2032, "sillHeight": 0, "swing": "left-in" }],
    "rooms": [{ "id": "R1", "name": "Office", "type": "office", "boundaryWallIds": ["W1"] }]
  }],
  "measurements": [{ "targetId": "W1", "field": "length", "value": 4270, "source": "laser" }]
}
```

Keeping `confidence` on scanned elements lets the editor highlight walls and openings the user should double-check. Keeping `measurements` separate from geometry preserves an audit trail of what was scanned versus what was measured. Design the schema so curved walls (arc segments), roofs, exterior elements, and furniture objects can be added later without breaking existing files.

## SketchUp modeling conventions

The generated model must look like a professional built it by hand: clean groups, reusable components, consistent tags, and nothing loose at the top level. These are starting defaults; validate them with at least one working interior designer before locking them in.

**Group hierarchy**

- `House` (group)
  - `Level 1 - Main Floor` (group, one per level)
    - `Walls` (group) containing one solid group per wall, named `Wall - Office North`
    - `Floors` (group) containing one face group per room, named `Floor - Kitchen`
    - `Ceilings` (group) containing one group per room
    - `Doors` and `Windows` containing component instances
- `Scan Reference` (group, hidden by default) holding the raw imported mesh when available

**Tags**

| Tag | Contents | Default visibility |
| --- | --- | --- |
| Walls | All wall groups | On |
| Floors | Floor faces | On |
| Ceilings | Ceiling faces | Off (so interiors are visible from above) |
| Doors | Door components | On |
| Windows | Window components | On |
| Dimensions | Dimension annotations | On |
| Scan Reference | Raw scan mesh | Off |

**Rules**

- Raw geometry stays on Untagged; tags go on groups and components only (standard SketchUp practice).
- Doors and windows are components named by size, such as `Door 32x80` or `Window 36x48`, so identical sizes share one definition.
- Openings cut through walls; each wall stays a watertight solid after cutting.
- Every wall gets an aligned dimension on the Dimensions tag.
- Model axes: origin at a front corner of the house, green axis along the front wall; north set through SketchUp's geo-location.
- Room names come from the HousePlan so designers can find spaces in the Outliner immediately.

## Input pipelines

Each input is a separate converter that outputs a HousePlan; none of them talk to SketchUp directly.

| Input | How it works | Accuracy expectation | Phase |
| --- | --- | --- | --- |
| LiDAR scan | RoomPlan `RoomCaptureSession` per room, merged with `StructureBuilder`; convert `CapturedStructure` walls, doors, windows, openings to HousePlan | Close enough that only spot checks are needed; confirm against a laser | v1 |
| Third-party import | Parse RoomPlan JSON/USDZ exports first (many scanning apps produce them), then OBJ, then DXF 2D plans | Depends on source; mesh-only files need wall detection | v1 (RoomPlan formats), later (OBJ/DXF) |
| Manual and laser measurements | Tap a wall in the 2D editor and type a length; later, pair a Bluetooth laser measure and send readings to the selected wall | Exact; always overrides scan values | v1 (manual), later (Bluetooth) |
| Blueprint | User uploads PDF or photo; vision model extracts walls, doors, windows into HousePlan; user sets scale by marking one known dimension; review screen before generating | Good on clean architectural plans; hand-drawn or faded scans need heavy review | Later |
| Photo/video walkthrough | Infer room adjacency and rough layout from video for non-LiDAR phones; user must enter all dimensions | Layout only, not dimensions | Later; also the basis for exteriors, roofs, and curved walls |

**Measurement reconciliation.** When a user corrects one wall length, the editor must propagate the change sensibly: move the connected corner, keep adjoining walls attached, and flag rooms that no longer close. This is the hardest part of the editor; spec it with Claude Code before building.

**Bluetooth laser measures.** Leica DISTO and Bosch GLM lines offer Bluetooth connectivity. Check which vendors publish an SDK or documented protocol for third-party iOS apps before committing.

## Orientation and geolocation

Record the phone's compass heading during the scan and hand SketchUp the address; SketchUp's own geo-location then handles north, sun position, and shadows. No need to ask which way the front door faces.

- **During the scan:** store the device's true-north heading (CoreLocation, corrected for magnetic declination) relative to the scan's coordinate frame. Take several readings and average them; steel, appliances, and wiring skew compass readings indoors.
- **Address:** the user types the address; geocode it to latitude and longitude on device.
- **Fallback:** if heading is unreliable, show the plan over a satellite map and let the user rotate the outline to match the roof. Also allow a manual "front door faces" picker.
- **In SketchUp:** the importer sets the model's geo-location from lat/lon and rotates north by `northAngleDeg`, so SketchUp's shadow settings are correct immediately.

## Phased build plan

Build the SketchUp importer before the scanner. It is the product's differentiator, it can be tested with hand-written HousePlan files, and it runs on any Mac without a LiDAR device.

| Phase | Scope | Gate |
| --- | --- | --- |
| 0 · Foundations | Repo, HousePlan JSON Schema, Swift types, 3 hand-built fixture plans. Confirm Xcode and a LiDAR test device work with the owner's Mac | All fixtures validate against the schema in CI |
| 1 · SketchUp generator | HousePlan to grouped, tagged model: walls, openings, floors, ceilings. Dimensions, door/window components, geo-location and north | Owner's house, typed in by hand, opens in SketchUp Web matching all conventions |
| 2 · iOS capture | RoomPlan multi-room scan, merged, converted to HousePlan. Heading and address captured; HousePlan exported via share sheet | Main floor scanned; laser-checked walls within 2 in (5 cm) |
| 3 · 2D editor and measurements | Plan view; edit wall lengths with reconciliation; add/move openings. Low-confidence items highlighted; one-tap regenerate for SketchUp | Any wall edit keeps rooms closed; the model regenerates cleanly |
| 4 · Imports and exports | Import RoomPlan files from other apps, then OBJ and DXF. Export OBJ/DAE and a dimensioned 2D floor plan PDF | A scan from one other app goes end to end into SketchUp |
| Later | Blueprints, Bluetooth laser, multiple levels, stairs, curved walls, exteriors, roofs, furniture recognition, Ruby extension for Pro users, photo layout | — |

Each phase ends at its gate; do not start the next phase until the gate passes on the owner's own house. The 2 in (5 cm) scan tolerance is a proposed target to revisit after the first real scan.

## Risks and open questions

The biggest early risk is the older Mac: current Xcode versions require a recent macOS, and App Store builds require a recent Xcode. Check this before writing any iOS code.

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Older Mac can't run a current Xcode | Can't build or ship the iOS app | Phase 0: check the Mac's macOS version against current Xcode requirements. Fallbacks: a newer used Mac, a cloud Mac service, or Xcode Cloud. Phase 1 (Ruby) is unaffected |
| No LiDAR device for testing | Can't test capture | Confirm the owner has an iPhone Pro or iPad Pro with LiDAR; RoomPlan does not run in the Simulator |
| Scan accuracy varies (glass, mirrors, clutter, dark rooms) | Wrong walls or missing openings | Confidence scores, highlight low-confidence items, require laser spot checks on key walls |
| Wall-edit reconciliation is complex | Editor produces broken rooms | Spec the rules first; start with orthogonal walls only |
| SketchUp Ruby extension is desktop-only | iPad/web SketchUp users can't import | Accept for v1; direct .skp export later |
| SketchUp C SDK licensing and platform limits | Blocks direct .skp export | Review Trimble's developer terms before Phase "Later" |
| Indoor compass error | Wrong sun and shadows | Averaged readings plus satellite-map rotation fallback |
| Overlap with SketchUp's own Scan-to-Design (iPad, Labs) | Less differentiation | Compete on clean professional output, iPhone support, imports, and the correction workflow |

**Open questions for the owner**

- [ ] What macOS version is the Mac running, and which Xcode version does it support?
- [ ] Which LiDAR device is available for testing?
- [ ] Which SketchUp edition do target designers use (Pro desktop, Go, web, iPad)?
- [ ] Are the grouping and tag conventions above what designers expect, or is there a house standard?
- [ ] Personal tool first, or App Store product from the start (affects accounts, pricing, and backend)?

## Decisions log

**2026-09-27 (owner answers to first review)**

- **Output path swapped (Option A).** The owner uses SketchUp Free (web), which cannot run Ruby extensions. v1 output is now a desktop command-line generator that writes `.skp` directly with the SketchUp C SDK; the file opens in every SketchUp edition. The Ruby extension moves to "Later" for Pro users. The C SDK has no solid booleans, so the generator computes wall solids with opening holes itself (straight walls make this tractable). Confirmed in the C API docs: groups, component definitions, layers/tags with visibility, linear dimensions (`SUDimensionLinearCreate`), geo-location (`SULocationSetLatLong`), north (`SUShadowInfoSetValue` with `NorthAngle`). The SDK requires an access request at developer.sketchup.com; its license terms are only visible after that.
- **Development machine.** Windows for Phases 0–1; the owner switches to the Mac for iOS work. The Mac/Xcode check is deferred until then.
- **Test devices.** iPhone 13 Pro Max and iPhone 17 Pro Max (both LiDAR). The iPad mini has no LiDAR.
- **Personal tool first**, not an App Store product. No accounts or backend.
- **Wall geometry.** Walls are stored as centerlines plus thickness, so two rooms can share one wall. Users see and type **inside-face** measurements; each measurement records which face it refers to. Walls meet only at their endpoints (a T-junction splits the through wall).
- **Regional defaults.** Wall thickness and ceiling height defaults come from a regional table chosen by the address (US first: 4.5 in interior, 6.5 in exterior, 8 ft ceiling). All values are editable per wall/room.
- **Openings** are positioned by `offsetFromStart` to the **near edge** of the opening.
- **Open-plan rooms** use invisible separator walls (`kind: "separator"`) that bound rooms but never become geometry.
- **Measurements** live only in `measurements[]`; `Wall.measuredLength` is dropped.
- **Ceiling height.** `Level.floorToCeiling` is the default; `Room.ceilingHeight` is an optional override (great rooms, primary bedrooms).
- **Axes.** SketchUp standard: origin at the front-left exterior corner, red (+X) along the front wall, green (+Y) toward the back, blue (+Z) up. `northAngleDeg` is measured clockwise from +Y to true north; the generator converts it to SketchUp's North Angle.
- **Units.** Imperial display by default with a metric option; component names follow the display unit (`Door 32x80` or `Door 813x2032`).
- **Dimensions.** Blueprint-style: inside dimensions for every room plus overall exterior dimensions; users can edit either.

## Instructions for Claude Code

Start with Phase 0 and work one phase at a time, stopping at each gate for the owner to verify on their own house.

1. Save this document into the repo as `docs/PROJECT_BRIEF.md` and create a `CLAUDE.md` summarizing the architecture, conventions, and current phase.
2. Before any iOS work, check the Mac's macOS and Xcode versions and report whether they can build a RoomPlan app for a current iOS release.
3. Write the HousePlan JSON Schema and three fixture plans first: one rectangular room, one L-shaped room, and a three-room floor with a hallway. Every later component is tested against these.
4. Build the SketchUp .skp generator (C SDK) against the fixtures (was: Ruby importer). Include automated checks where possible (group names, tag assignments, wall counts, solid walls) and a manual test checklist for the owner.
5. Keep converters, the editor, and generators separated by the HousePlan format; never have the iOS app emit SketchUp geometry directly.
6. When a decision affects what designers will see in SketchUp (naming, grouping, tags, units), propose it and ask the owner rather than choosing silently.
7. Verify current Apple RoomPlan, SketchUp Ruby API, and SketchUp C SDK documentation before relying on any API; details in this brief may be out of date.
8. Update `CLAUDE.md` at the end of each phase with what was built, what changed in the schema, and what's next.

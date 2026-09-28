"""Validate HousePlan files against the JSON Schema plus geometry rules the schema can't express.

Usage:
    python schema/validate.py FILE_OR_DIR [...]                  # every file must be valid
    python schema/validate.py --expect-invalid FILE_OR_DIR [...]  # every file must be rejected

Exits 0 when every file meets the expectation, 1 otherwise.
"""

import json
import math
import sys
from pathlib import Path

from jsonschema import Draft202012Validator, FormatChecker

SCHEMA_PATH = Path(__file__).with_name("houseplan.schema.json")
TOLERANCE_MM = 1.0


def load_validator():
    schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
    Draft202012Validator.check_schema(schema)
    return Draft202012Validator(schema, format_checker=FormatChecker())


def dist(a, b):
    return math.hypot(a["x"] - b["x"], a["y"] - b["y"])


def same_point(a, b):
    return dist(a, b) <= TOLERANCE_MM


def point_strictly_inside_segment(p, a, b):
    """True if p lies on segment a-b but not within tolerance of either end."""
    length = dist(a, b)
    if length == 0 or same_point(p, a) or same_point(p, b):
        return False
    cross = (b["x"] - a["x"]) * (p["y"] - a["y"]) - (b["y"] - a["y"]) * (p["x"] - a["x"])
    if abs(cross) / length > TOLERANCE_MM:
        return False
    t = ((p["x"] - a["x"]) * (b["x"] - a["x"]) + (p["y"] - a["y"]) * (b["y"] - a["y"])) / length**2
    return 0 < t < 1


def check_level(level, errors):
    where = f"level {level['id']}"
    walls = {w["id"]: w for w in level["walls"]}

    seen = set()
    for item in level["walls"] + level["openings"] + level["rooms"]:
        if item["id"] in seen:
            errors.append(f"{where}: duplicate id {item['id']}")
        seen.add(item["id"])

    for w in level["walls"]:
        if dist(w["start"], w["end"]) <= TOLERANCE_MM:
            errors.append(f"{where}: wall {w['id']} has zero length")
        for other in level["walls"]:
            if other is w:
                continue
            for end in ("start", "end"):
                if point_strictly_inside_segment(other[end], w["start"], w["end"]):
                    errors.append(
                        f"{where}: wall {other['id']} meets the middle of wall {w['id']}; split {w['id']} at that point"
                    )

    # Effective ceiling for each wall = tallest room it bounds, else the level default.
    wall_ceiling = {wid: level["floorToCeiling"] for wid in walls}
    for room in level["rooms"]:
        ceiling = room.get("ceilingHeight", level["floorToCeiling"])
        for wid in room["boundaryWallIds"]:
            if wid in wall_ceiling:
                wall_ceiling[wid] = max(wall_ceiling[wid], ceiling)

    by_wall = {}
    for o in level["openings"]:
        w = walls.get(o["wallId"])
        if w is None:
            errors.append(f"{where}: opening {o['id']} references missing wall {o['wallId']}")
            continue
        if w.get("kind") == "separator":
            errors.append(f"{where}: opening {o['id']} is on separator {w['id']}; separators have no geometry")
        length = dist(w["start"], w["end"])
        if o["offsetFromStart"] + o["width"] > length + TOLERANCE_MM:
            errors.append(f"{where}: opening {o['id']} runs past the end of wall {w['id']} ({length:.0f} mm long)")
        height = w.get("height", wall_ceiling[w["id"]])
        if o["sillHeight"] + o["height"] > height + TOLERANCE_MM:
            errors.append(f"{where}: opening {o['id']} is taller than wall {w['id']} ({height:.0f} mm)")
        if o["type"] == "door" and o["sillHeight"] != 0:
            errors.append(f"{where}: door {o['id']} has a non-zero sill height")
        by_wall.setdefault(w["id"], []).append(o)

    for wid, ops in by_wall.items():
        ops = sorted(ops, key=lambda o: o["offsetFromStart"])
        for a, b in zip(ops, ops[1:]):
            if a["offsetFromStart"] + a["width"] > b["offsetFromStart"] + TOLERANCE_MM:
                errors.append(f"{where}: openings {a['id']} and {b['id']} overlap on wall {wid}")

    for room in level["rooms"]:
        ids = room["boundaryWallIds"]
        missing = [wid for wid in ids if wid not in walls]
        if missing:
            errors.append(f"{where}: room {room['id']} references missing walls {missing}")
            continue
        if len(set(ids)) != len(ids):
            errors.append(f"{where}: room {room['id']} lists a wall twice")
        for i, wid in enumerate(ids):
            a, b = walls[wid], walls[ids[(i + 1) % len(ids)]]
            if not any(same_point(p, q) for p in (a["start"], a["end"]) for q in (b["start"], b["end"])):
                errors.append(f"{where}: room {room['id']} does not close: {a['id']} and {b['id']} don't share an endpoint")

    return seen


def check_plan(plan, validator):
    errors = [
        f"schema: {'/'.join(str(p) for p in e.absolute_path) or '(root)'}: {e.message}"
        for e in sorted(validator.iter_errors(plan), key=lambda e: list(e.absolute_path))
    ]
    if errors:
        return errors  # geometry checks assume a schema-valid document

    all_ids = set()
    for level in plan["levels"]:
        all_ids |= check_level(level, errors)
    for m in plan.get("measurements", []):
        if m["targetId"] not in all_ids:
            errors.append(f"measurement {m.get('id', '?')} targets missing id {m['targetId']}")
        if "roomId" in m and m["roomId"] not in all_ids:
            errors.append(f"measurement {m.get('id', '?')} references missing room {m['roomId']}")
    return errors


def collect(paths):
    for p in map(Path, paths):
        yield from sorted(p.glob("*.json")) if p.is_dir() else [p]


def main(argv):
    expect_invalid = "--expect-invalid" in argv
    paths = [a for a in argv if a != "--expect-invalid"]
    if not paths:
        print(__doc__)
        return 2

    validator = load_validator()
    ok = True
    for path in collect(paths):
        try:
            errors = check_plan(json.loads(path.read_text(encoding="utf-8")), validator)
        except json.JSONDecodeError as e:
            errors = [f"invalid JSON: {e}"]
        if expect_invalid:
            if errors:
                print(f"PASS (rejected) {path}: {errors[0]}")
            else:
                ok = False
                print(f"FAIL {path}: expected to be rejected but it validated")
        elif errors:
            ok = False
            print(f"FAIL {path}")
            for e in errors:
                print(f"  - {e}")
        else:
            print(f"PASS {path}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

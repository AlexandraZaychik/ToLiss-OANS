"""Read-only geometry audit against an installed Magknight 1.11.9 aircraft.

No aircraft files are copied or changed. Run separately from the portable suite:
  python scripts/verify-magknight-profiles.py <aircraft-directory> --report <json>
"""
import argparse
import hashlib
import json
import math
from pathlib import Path


def properties(path):
    result = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = line.split(maxsplit=2)
        if len(parts) == 3 and parts[0] == "P":
            result[parts[1]] = parts[2].strip()
    return result


def panel_vertices(path):
    vertices, indices, used = [], [], set()
    cockpit = False
    for line in path.read_text(encoding="utf-8").splitlines():
        p = line.split()
        if not p:
            continue
        if p[0] == "VT":
            vertices.append(list(map(float, p[1:])))
        elif p[0] in ("IDX", "IDX10"):
            indices.extend(map(int, p[1:]))
        elif p[0] in ("ATTR_cockpit", "ATTR_cockpit_region"):
            cockpit = True
        elif p[0] == "ATTR_no_cockpit":
            cockpit = False
        elif p[0] == "TRIS" and cockpit:
            start, count = map(int, p[1:3])
            used.update(indices[start:start + count])
    return [vertices[i] for i in sorted(used)]


def audit(aircraft, config):
    panel = "common/cockpit/obj/cmn_cockpit_1.obj"
    obj = aircraft / "objects" / panel
    vertices = panel_vertices(obj)
    result = {"aircraft_directory": str(aircraft.resolve()),
              "shared_cockpit": panel,
              "cockpit_sha256": hashlib.sha256(obj.read_bytes()).hexdigest(),
              "models": [], "simulator_started": False}
    for model, suffix, icao in (("B788", "8", "B788"),
                                ("B789", "9", "B789"),
                                ("B78X", "10", "B789")):
        acf = aircraft / (model + ".acf")
        p = properties(acf)
        assert p["acf/_ICAO"] == icao, (model, "unexpected authored ICAO")
        assert "Magknight" in p["acf/_author"]
        assert p["acf/_name"].startswith("Magknight 787-" + suffix + " ")
        attachments = [k.rsplit("/", 1)[0] for k, v in p.items()
                       if k.endswith("/_v10_att_file_stl") and v == panel]
        assert len(attachments) == 1, (model, "shared panel attachment missing/ambiguous")
        attachment = attachments[0] + "/_v10_att_"
        assert all(float(p[attachment + a + "_ref"]) == 0 for a in ("phi", "psi", "the"))
        offset = [float(p[attachment + a + "_acf_prt_ref"]) * .3048 for a in "xyz"]
        offset[1] -= float(p["acf/_cgY"]) * .3048
        offset[2] -= float(p["acf/_cgZ"]) * .3048
        profile = json.loads((config / (model.lower() + ".json")).read_text())
        matched, errors = [], []
        for corner in profile["nd_corners"][0]:
            raw = [corner[a] - offset[a] for a in range(3)]
            v = min(vertices, key=lambda candidate: math.dist(candidate[:3], raw))
            error = math.dist(v[:3], raw)
            assert error < 2e-6, (model, "click corner differs from panel", error)
            matched.append(v)
            errors.append(error)
        uv = profile["nd_uv"][0][0]
        measured = [min(v[6] for v in matched), min(v[7] for v in matched),
                    max(v[6] for v in matched), max(v[7] for v in matched)]
        assert max(abs(a - b) for a, b in zip(uv, measured)) < 2e-6
        fo_matched = []
        for corner in profile["nd_corners"][1]:
            raw = [corner[a] - offset[a] for a in range(3)]
            v = min(vertices, key=lambda candidate: math.dist(candidate[:3], raw))
            assert math.dist(v[:3], raw) < 2e-6, (model, "FO corner differs from panel")
            fo_matched.append(v)
        fo_uv = profile["nd_uv"][0][1]
        fo_measured = [min(v[6] for v in fo_matched), min(v[7] for v in fo_matched),
                       max(v[6] for v in fo_matched), max(v[7] for v in fo_matched)]
        assert max(abs(a - b) for a, b in zip(fo_uv, fo_measured)) < 2e-6
        result["models"].append({"model": model, "authored_icao": icao,
                                  "acf_sha256": hashlib.sha256(acf.read_bytes()).hexdigest(),
                                  "cg_y_ft": float(p["acf/_cgY"]), "cg_z_ft": float(p["acf/_cgZ"]),
                                  "nd_uv": uv, "nd_corners": profile["nd_corners"][0],
                                  "max_corner_error_m": max(errors)})
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("aircraft_directory", type=Path)
    parser.add_argument("--config", type=Path, default=Path(__file__).resolve().parents[1] / "config/magknight")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    report = audit(args.aircraft_directory, args.config)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for entry in report["models"]:
        print(f"PASS {entry['model']} shared panel UV and CG-relative click corners; "
              f"max error {entry['max_corner_error_m']:.3g} m")

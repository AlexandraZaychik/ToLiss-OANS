"""Read-only profile extraction. Never copies aircraft assets or reads credentials."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
DEFAULT = Path(r"G:\SteamLibrary\steamapps\common\X-Plane 12\Aircraft\ToLissA320_V1p3p2")

def audit(aircraft: Path) -> dict:
    obj = aircraft / "cockpit_INN.obj"
    data = obj.read_bytes()
    vertices, indices, screens = [], [], {}
    # These six-screen batches are identified by the display-resolution hide conditions.
    conditions = []
    for line_no, line in enumerate(data.decode("utf-8").splitlines(), 1):
        t = line.split()
        if not t:
            continue
        if t[0] == "VT":
            vertices.append(list(map(float, t[1:])))
        elif t[0] in ("IDX", "IDX10"):
            indices.extend(map(int, t[1:]))
        elif t[0] == "ANIM_begin":
            conditions.append([])
        elif t[0] == "ANIM_end":
            conditions.pop()
        elif t[0] == "ANIM_hide" and conditions:
            conditions[-1].append(t[1:])
        elif t[0] == "TRIS" and t[2] == "36":
            cond = [v for block in conditions for v in block if v[-1] == "AirbusFBW/DisplayResolutionFallback"]
            if not cond:
                continue
            mode = "normal" if float(cond[-1][0]) > 0 else "fallback"
            batch = []
            for offset in range(int(t[1]), int(t[1]) + 36, 6):
                unique = {tuple(vertices[indices[i]][j] for j in (0, 1, 2, 6, 7)) for i in range(offset, offset + 6)}
                if len(unique) != 4:
                    raise ValueError("Expected four screen corners")
                v = sorted(unique, key=lambda p: (-p[4], p[3]))
                corners = [v[0], v[1], v[3], v[2]]
                batch.append({"uv": [min(p[3] for p in v), min(p[4] for p in v), max(p[3] for p in v), max(p[4] for p in v)],
                              "obj_corners": [list(p[:3]) for p in corners]})
            screens[mode] = {"line": line_no, "screens": batch}
    if set(screens) != {"normal", "fallback"}:
        raise ValueError("Unknown display layout; refusing to generate a guessed profile")
    brightness, pfd_brightness = [], []
    obj_text = data.decode("utf-8")
    for side in ("Left", "Right"):
        pattern = rf"ckpt/lights/ndInner{side}/anim.*?AirbusFBW/DUBrightness\[(\d+)\]"
        match = re.search(pattern, obj_text, re.S)
        if not match:
            raise ValueError(f"ND brightness mapping missing: {side}")
        brightness.append(int(match[1]))
        # Captain PFD manipulator is unnamed; FO is pfdRight/anim.
        # Verified indices in the inspected object (53584 / 53875).
        pfd_index = 0 if side == 'Left' else 2
        if not re.search(rf"ATTR_manip_drag_axis[^\n]*AirbusFBW/DUBrightness\[{pfd_index}\]", obj_text):
            raise ValueError(f"PFD brightness mapping missing: {side}")
        pfd_brightness.append(pfd_index)
    models = []
    for name in ("a320.acf", "a320_StdDef.acf"):
        acf_data = (aircraft / name).read_bytes()
        properties = {}
        for line in acf_data.decode("utf-8").splitlines():
            if line.startswith("P "):
                _, key, value = line.split(" ", 2)
                properties[key] = value
        if properties["acf/_ICAO"] != "A20N":
            raise ValueError("This profile is only for the inspected ToLiss A20N")
        offset = [float(properties[f"_obja/0/_v10_att_{axis}_acf_prt_ref"]) * .3048 for axis in "xyz"]
        offset[1] -= float(properties["acf/_cgY"]) * .3048
        offset[2] -= float(properties["acf/_cgZ"]) * .3048
        if not properties["_obja/0/_v10_att_file_stl"].endswith("cockpit_INN.obj"):
            raise ValueError("Unknown cockpit attachment")
        if any(float(properties[f"_obja/0/_v10_att_{key}_ref"]) != 0 for key in ("phi", "psi", "the")):
            raise ValueError("Rotated cockpit attachment needs a new transform")
        models.append({"name": name, "object_to_cg": offset})
    if models[0]["object_to_cg"] != models[1]["object_to_cg"]:
        raise ValueError("Different model coordinate systems")
    offset = models[0]["object_to_cg"]
    for mode in screens.values():
        for screen in mode["screens"]:
            screen["cg_corners"] = [[p[i] + offset[i] for i in range(3)] for p in screen["obj_corners"]]
    system = aircraft / "plugins/AirbusFBW_A320_XP11/win_x64/AirbusFBW_A320_XP11.xpl"
    strings = re.findall(rb"[ -~]{5,}", system.read_bytes())
    refs = sorted({s.decode() for s in strings if re.fullmatch(rb"(?:AirbusFBW|toliss_airbus)/[a-zA-Z0-9_/]+", s)})
    return {"aircraft": str(aircraft), "object": {"name": obj.name},
            "models": models, "displays": screens, "nd_brightness_indices": brightness, "pfd_brightness_indices": pfd_brightness, "static_interface_candidates": refs,
            "runtime_verified": False, "note": "Binary string presence is not a type, writability, or semantics guarantee."}

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--aircraft", type=Path, default=DEFAULT)
    args = parser.parse_args()
    result = audit(args.aircraft)
    report = PROJECT / "reports/aircraft_audit.json"
    report.parent.mkdir(exist_ok=True)
    report.write_bytes(json.dumps(result, ensure_ascii=False, indent=2).replace("\n", "\r\n").encode("utf-8"))
    profile = {"nd_brightness": result["nd_brightness_indices"],
               "pfd_brightness": result["pfd_brightness_indices"]}
    for key, indices in (("nd_uv", (1, 3)), ("pfd_uv", (0, 2))):
        profile[key] = [[result["displays"][mode]["screens"][i]["uv"] for i in indices]
                        for mode in ("normal", "fallback")]
    profile["nd_corners"] = [result["displays"]["normal"]["screens"][i]["cg_corners"] for i in (1, 3)]
    profile_path = PROJECT / "config/a20n.json"
    profile_path.parent.mkdir(exist_ok=True)
    profile_path.write_text(json.dumps(profile, indent=2) + "\n", encoding="utf-8")
    print(f"Profile extracted: {report}")
    print("Normal ND sizes:", [(round((s["uv"][2]-s["uv"][0])*4096), round((s["uv"][3]-s["uv"][1])*4096)) for s in result["displays"]["normal"]["screens"]])
    print("Fallback ND sizes:", [(round((s["uv"][2]-s["uv"][0])*4096), round((s["uv"][3]-s["uv"][1])*4096)) for s in result["displays"]["fallback"]["screens"]])

if __name__ == "__main__":
    main()

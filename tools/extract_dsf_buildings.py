"""Extract airport building footprints from facade polygons in an X-Plane DSF.

The runtime consumes the resulting JSON cache. DSFTool is used only by this
offline converter; the simulator plugin does not parse binary DSF files.
"""
from __future__ import annotations

import argparse
import json
import math
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


METERS_PER_DEGREE = 111_320.0
EXCLUDED_RESOURCE_WORDS = (
    "bridge", "jetway", "fence", "roof", "canopy", "awning", "interior",
)


def _valid_lat_lon(lat: float, lon: float) -> bool:
    return math.isfinite(lat) and math.isfinite(lon) and abs(lat) <= 90 and abs(lon) <= 180


def airport_bounds(apt_dat: Path, icao: str, margin_m: float) -> dict[str, float]:
    """Read the matching apt.dat airport and return a padded WGS84 bbox."""
    current: dict | None = None

    def finish(block: dict | None) -> list[tuple[float, float]] | None:
        if not block:
            return None
        code = block.get("explicit_icao") or block.get("header_id")
        if code != icao:
            return None
        if not block["points"]:
            raise ValueError(f"Airport {icao} has no usable runway or layout coordinates")
        return block["points"]

    points = None
    with apt_dat.open("r", encoding="utf-8-sig", errors="replace") as source:
        for raw in source:
            row = raw.split()
            if not row:
                continue
            if row[0] in {"1", "16", "17"} and len(row) > 4:
                points = finish(current)
                if points is not None:
                    break
                current = {"header_id": row[4].upper(), "explicit_icao": None, "points": []}
                continue
            if current is None:
                continue
            if row[0] == "99":
                points = finish(current)
                if points is not None:
                    break
                current = None
                continue
            if row[0] == "1302" and len(row) > 2 and row[1] == "icao_code":
                current["explicit_icao"] = row[2].upper()
                continue

            def add_pair(lat_index: int, lon_index: int) -> None:
                if len(row) <= max(lat_index, lon_index):
                    return
                try:
                    lat, lon = float(row[lat_index]), float(row[lon_index])
                except ValueError:
                    return
                if _valid_lat_lon(lat, lon):
                    current["points"].append((lat, lon))

            if row[0] == "100":
                add_pair(9, 10)
                add_pair(18, 19)
            elif row[0] in {"111", "112", "113", "114"}:
                add_pair(1, 2)
                if row[0] in {"112", "114"}:
                    add_pair(3, 4)
            elif row[0] == "1201":
                add_pair(1, 2)
            elif row[0] == "1300":
                add_pair(1, 2)

    if points is None:
        points = finish(current)
    if points is None:
        raise ValueError(f"ICAO {icao} was not found in {apt_dat}")
    if margin_m < 0:
        raise ValueError("margin_m must be non-negative")

    lats = [p[0] for p in points]
    lons = [p[1] for p in points]
    lat0 = sum(lats) / len(lats)
    lat_margin = margin_m / METERS_PER_DEGREE
    lon_margin = margin_m / (METERS_PER_DEGREE * max(0.01, math.cos(math.radians(lat0))))
    return {
        "min_lat": min(lats) - lat_margin,
        "max_lat": max(lats) + lat_margin,
        "min_lon": min(lons) - lon_margin,
        "max_lon": max(lons) + lon_margin,
        "margin_m": margin_m,
    }


def is_building_facade(resource: str) -> tuple[bool, str | None]:
    path = resource.replace("\\", "/").lower()
    if not path.endswith(".fac") or any(word in path for word in EXCLUDED_RESOURCE_WORDS):
        return False, None
    if "/terminal_kit/term_building_" in path or "/facades/terminal_" in path:
        return True, "terminal"
    if path.startswith(("lib/airport/buildings/", "lib/airport/hangars/", "lib/buildings/facades/")):
        return True, "building"
    return False, None


def _ring_area_m2(points: list[tuple[float, float]]) -> float:
    if len(points) < 3:
        return 0.0
    lat0 = math.radians(sum(lat for lat, _ in points) / len(points))
    lon0 = sum(lon for _, lon in points) / len(points)
    xy = [((lon - lon0) * METERS_PER_DEGREE * math.cos(lat0), (lat - math.degrees(lat0)) * METERS_PER_DEGREE)
          for lat, lon in points]
    area2 = sum(xy[i][0] * xy[(i + 1) % len(xy)][1] - xy[(i + 1) % len(xy)][0] * xy[i][1]
                for i in range(len(xy)))
    return abs(area2) * 0.5


def _flatten_winding(points: list[list[float]], samples_per_curve: int = 12) -> list[tuple[float, float]]:
    """Flatten DSF facade Bezier handles into lon/lat points.

    DSF facade points may contain lon/lat, optional wall choice, and optional
    Bezier lon/lat. As in apt.dat, a point's handle is the incoming handle;
    the outgoing handle is mirrored around that vertex.
    """
    vertices: list[tuple[float, float, tuple[float, float]]] = []
    for values in points:
        if len(values) < 2:
            continue
        lon, lat = values[0], values[1]
        if not _valid_lat_lon(lat, lon):
            continue
        has_curve = len(values) >= 4
        control = (values[-2], values[-1]) if has_curve else (lon, lat)
        if not _valid_lat_lon(control[1], control[0]):
            control = (lon, lat)
        vertices.append((lon, lat, control))

    if len(vertices) > 1 and abs(vertices[0][0] - vertices[-1][0]) < 1e-10 and abs(vertices[0][1] - vertices[-1][1]) < 1e-10:
        vertices.pop()
    if len(vertices) < 3:
        return []

    out = [(vertices[0][0], vertices[0][1])]
    for i, start in enumerate(vertices):
        j = (i + 1) % len(vertices)
        end = vertices[j]
        p0 = (start[0], start[1])
        p3 = (end[0], end[1])
        c1 = (2 * p0[0] - start[2][0], 2 * p0[1] - start[2][1])
        c2 = end[2]
        curved = (abs(c1[0] - p0[0]) > 1e-10 or abs(c1[1] - p0[1]) > 1e-10 or
                  abs(c2[0] - p3[0]) > 1e-10 or abs(c2[1] - p3[1]) > 1e-10)
        if curved:
            for step in range(1, samples_per_curve + 1):
                if j == 0 and step == samples_per_curve:
                    continue
                t = step / samples_per_curve
                u = 1.0 - t
                lon = u**3 * p0[0] + 3 * u**2 * t * c1[0] + 3 * u * t**2 * c2[0] + t**3 * p3[0]
                lat = u**3 * p0[1] + 3 * u**2 * t * c1[1] + 3 * u * t**2 * c2[1] + t**3 * p3[1]
                out.append((lon, lat))
        elif j != 0:
            out.append(p3)
    out.append(out[0])  # The existing map geometry joiner expects explicit closure.
    return out


def extract_buildings(text_path: Path, icao: str, bounds: dict[str, float], min_area_m2: float) -> tuple[list[dict], dict[str, int]]:
    definitions: list[str] = []
    buildings: list[dict] = []
    counts = {"building_facade_candidates": 0, "ignored_instances": 0, "outside_bounds": 0, "below_min_area": 0, "kept": 0}
    current: dict | None = None
    ring: list[list[float]] | None = None

    def finish_polygon(poly: dict) -> None:
        resource = poly["resource"]
        keep, kind = is_building_facade(resource)
        if not keep:
            counts["ignored_instances"] += 1
            return
        counts["building_facade_candidates"] += 1
        rings = poly["rings"]
        if not rings:
            return
        # Facades cannot contain holes; use the largest winding if the input
        # contains an unexpected multi-winding facade.
        candidates = [_flatten_winding(r) for r in rings]
        candidates = [r for r in candidates if len(r) >= 3]
        if not candidates:
            return
        points = max(candidates, key=_ring_area_m2)
        min_lon, max_lon = min(p[0] for p in points), max(p[0] for p in points)
        min_lat, max_lat = min(p[1] for p in points), max(p[1] for p in points)
        if (max_lon < bounds["min_lon"] or min_lon > bounds["max_lon"] or
                max_lat < bounds["min_lat"] or min_lat > bounds["max_lat"]):
            counts["outside_bounds"] += 1
            return
        area = _ring_area_m2(points)
        if area < min_area_m2:
            counts["below_min_area"] += 1
            return
        buildings.append({
            "kind": kind,
            "resource": resource,
            "facade_height_m": poly["height_m"],
            "area_m2": round(area, 1),
            "points": [[round(lon, 9), round(lat, 9)] for lon, lat in points],
        })
        counts["kept"] += 1

    with text_path.open("r", encoding="utf-8", errors="replace") as source:
        for raw in source:
            row = raw.split()
            if not row:
                continue
            if row[0] == "POLYGON_DEF" and len(row) >= 2:
                definitions.append(" ".join(row[1:]))
            elif row[0] == "BEGIN_POLYGON" and len(row) >= 3:
                try:
                    definition_index = int(row[1])
                    height_m = float(row[2])
                except ValueError:
                    current = None
                    continue
                resource = definitions[definition_index] if 0 <= definition_index < len(definitions) else ""
                current = {"resource": resource, "height_m": height_m, "rings": []}
                ring = None
            elif current is not None and row[0] == "BEGIN_WINDING":
                ring = []
                current["rings"].append(ring)
            elif current is not None and ring is not None and row[0] == "POLYGON_POINT":
                try:
                    ring.append([float(v) for v in row[1:]])
                except ValueError:
                    continue
            elif current is not None and row[0] == "END_WINDING":
                ring = None
            elif current is not None and row[0] == "END_POLYGON":
                finish_polygon(current)
                current = None
                ring = None
    return buildings, counts


def find_dsftool(explicit: Path | None) -> Path:
    if explicit:
        if not explicit.is_file():
            raise FileNotFoundError(f"DSFTool not found: {explicit}")
        return explicit
    for name in ("DSFTool.exe", "DSFTool"):
        found = shutil.which(name)
        if found:
            return Path(found)
    raise FileNotFoundError("DSFTool not found; pass --dsftool with the X-Plane xptools path")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("icao", help="Airport ICAO code")
    parser.add_argument("--dsf", required=True, type=Path, help="Binary DSF tile containing the airport")
    parser.add_argument("--apt-dat", required=True, type=Path, help="X-Plane Global Airports apt.dat")
    parser.add_argument("--dsftool", type=Path, help="Path to the official X-Plane DSFTool executable")
    parser.add_argument("--output", type=Path, default=Path("cache"), help="Output directory (default: cache)")
    parser.add_argument("--margin-m", type=float, default=800.0, help="Padding around apt.dat bounds in meters")
    parser.add_argument("--min-area-m2", type=float, default=150.0, help="Discard smaller footprints")
    args = parser.parse_args()
    icao = args.icao.upper()
    if not re.fullmatch(r"[A-Z0-9]{3,8}", icao):
        parser.error("Invalid airport code")
    if not args.dsf.is_file():
        parser.error(f"DSF not found: {args.dsf}")
    if not args.apt_dat.is_file():
        parser.error(f"apt.dat not found: {args.apt_dat}")
    if args.min_area_m2 < 0:
        parser.error("--min-area-m2 must be non-negative")
    dsftool = find_dsftool(args.dsftool)
    bounds = airport_bounds(args.apt_dat, icao, args.margin_m)

    with tempfile.TemporaryDirectory(prefix=f"oans-{icao.lower()}-dsf-") as scratch:
        text_path = Path(scratch) / f"{args.dsf.stem}.txt"
        command = [str(dsftool), "--dsf2text", str(args.dsf), str(text_path)]
        converted = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
        if converted.returncode != 0 or not text_path.is_file():
            details = (converted.stderr or converted.stdout).strip()
            raise RuntimeError(f"DSFTool conversion failed ({converted.returncode}): {details}")
        buildings, counts = extract_buildings(text_path, icao, bounds, args.min_area_m2)

    if not buildings:
        raise RuntimeError(f"No building facades selected for {icao}; inspect DSF, apt.dat bounds, and resource filters")
    output_path = args.output / f"dsf_buildings_{icao}.json"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema": "oans-dsf-buildings-v1",
        "icao": icao,
        "source": "x-plane-dsf-facade",
        "dsf_tile": args.dsf.stem,
        "bounds": bounds,
        "filter": {"min_area_m2": args.min_area_m2},
        "counts": counts,
        "buildings": buildings,
    }
    output_path.write_text(json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"{output_path}: {counts['kept']} footprints ({sum(b['kind'] == 'terminal' for b in buildings)} terminals), "
          f"{output_path.stat().st_size} bytes; DSF tile {args.dsf.stem}")


if __name__ == "__main__":
    main()

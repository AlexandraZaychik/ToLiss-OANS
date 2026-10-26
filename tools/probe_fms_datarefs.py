"""Read-only ToLiss FMS DataRef discovery using X-Plane's local Web API.

Standard library only. GET requests only: never changes MCDU pages, flight
plans, cameras, DataRefs, commands, or plugins. IDs are resolved every run.
"""
from __future__ import annotations

import argparse
import base64
from datetime import datetime, timezone, timedelta
import json
from pathlib import Path
import re
import time
import urllib.request


ROOT = Path(__file__).resolve().parents[1]
FMS_CANDIDATES = (
    "AirbusFBW/fms/init/fromName", "AirbusFBW/fms/dep/from",
    "AirbusFBW/fms/init/toName", "AirbusFBW/fms/arr/to",
    "AirbusFBW/fms/report/destination/name", "AirbusFBW/fms/arr/rwy",
    "AirbusFBW/fms/arr/app", "AirbusFBW/fms/init/altnName",
    "AirbusFBW/fms/init/altn", "AirbusFBW/fms/dep/rwy",
    "toliss_airbus/flightplan/departure_icao",
    "toliss_airbus/flightplan/destination_icao",
    "toliss_airbus/flightplan/alternate_icao",
    "toliss_airbus/flightplan/no_wp", "toliss_airbus/flightplan/wpid",
    "toliss_airbus/flightplan/dest_index",
    "toliss_airbus/departure/sid_queryElementName",
    "toliss_airbus/arrival/star_queryElementName",
)
CONTEXT_REFS = (
    "sim/aircraft/view/acf_ICAO", "sim/flightmodel/position/latitude",
    "sim/flightmodel/position/longitude", "sim/flightmodel/position/groundspeed",
    "sim/flightmodel2/gear/on_ground", "sim/time/paused",
    "AirbusFBW/QPACFlightPhase", "AirbusFBW/ECAMFlightPhase",
    "AirbusFBW/DistToDest", "AirbusFBW/DistToDest_FO",
    "AirbusFBW/WPT_ID", "AirbusFBW/WPT_ID_FO",
)
KEYWORDS = re.compile(r"fms|fmg|flightplan|runway|rwy|departure|arrival|destination|altn|flightphase", re.I)
MCDU_TEXT = re.compile(r"AirbusFBW/MCDU[12](?:s?title|label|s?cont)\w*\Z")
AIRPORT_RUNWAY = re.compile(r"^([A-Z][A-Z0-9]{3})(0[1-9]|[12][0-9]|3[0-6])([LCR]?)(?=\s|$)")


def now() -> str:
    return datetime.now(timezone(timedelta(hours=8))).isoformat(timespec="milliseconds")


class ReadOnlyAPI:
    def __init__(self, base_url: str):
        self.base = base_url.rstrip("/")
        # Local simulator requests must not be routed through a system proxy.
        self.opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def get(self, path: str):
        request = urllib.request.Request(self.base + path, headers={"Accept": "application/json"}, method="GET")
        with self.opener.open(request, timeout=5) as response:
            return json.load(response)

    def read(self, meta: dict) -> dict:
        value = self.get(f'/api/v2/datarefs/{meta["id"]}/value')["data"]
        result = {"value": value}
        if meta["value_type"] == "data":
            raw = base64.b64decode(value, validate=True)
            result.update(byte_length=len(raw), text=raw.split(b"\0", 1)[0].decode("utf-8", errors="replace"))
        return result


def page_observations(values: dict, side: int) -> dict:
    """Interpret only explicit FROM/DEST rows; never treat an arbitrary fix as origin."""
    prefix = f"AirbusFBW/MCDU{side}"

    def texts(stem: str):
        return [(name, v.get("text", "")) for name, v in values.items() if name.startswith(prefix + stem)]

    result = {"source": "current MCDU screen text; page dependent", "origin": None, "destination": None}
    # Do not accept yellow TEMPY data as the active flight plan. The supported
    # observation here is the explicitly labelled, non-TEMPY F-PLN layout.
    tempy = any("TMPY" in t or "TEMPY" in t for _, t in texts("title") + texts("stitle"))
    from_label = any(t.strip() == "FROM" for _, t in texts("stitle"))
    dest_label = any(t.lstrip().startswith("DEST") and "TIME" in t and "DIST" in t for _, t in texts("label6"))
    for role, label_ok, stem in (("origin", from_label, "cont1"), ("destination", dest_label, "cont6")):
        if not label_ok or tempy:
            continue
        for name, text in texts(stem):
            if name[-1] not in "gw":
                continue
            match = AIRPORT_RUNWAY.match(text)
            if match:
                result[role] = {"icao": match[1], "runway": match[2] + match[3], "dataref": name, "raw_text": text}
                break
    result["guards"] = {"from_label": from_label, "destination_footer": dest_label, "temporary_plan_title": tempy}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", default="http://127.0.0.1:8086")
    parser.add_argument("--output", type=Path, default=ROOT / "reports/runtime/fms-datarefs-20261023.json")
    parser.add_argument("--samples", type=int, default=2)
    args = parser.parse_args()
    if not 1 <= args.samples <= 10:
        parser.error("--samples must be between 1 and 10")
    api = ReadOnlyAPI(args.base_url)
    capabilities = api.get("/api/capabilities")
    registry = api.get("/api/v2/datarefs")["data"]
    by_name = {d["name"]: d for d in registry}
    candidates = {n: {"present": n in by_name, "metadata": by_name.get(n)} for n in FMS_CANDIDATES}
    relevant = [d for d in registry if d["name"].startswith(("AirbusFBW/", "toliss_airbus/")) and KEYWORDS.search(d["name"])]
    selected = [d for d in registry if d["name"] in CONTEXT_REFS or d["name"] in FMS_CANDIDATES
                or (d["value_type"] == "data" and MCDU_TEXT.fullmatch(d["name"]))]
    report = {"captured_at": now(), "scope": "GET only; current registered DataRefs and current MCDU text, not a full FMS adapter",
              "capabilities": capabilities, "registry_count": len(registry), "candidate_presence": candidates,
              "registered_related_metadata": relevant, "selected_metadata": selected, "samples": []}
    for index in range(args.samples):
        sample = {"started_at": now(), "values": {}, "errors": {}}
        for meta in selected:
            try:
                sample["values"][meta["name"]] = api.read(meta)
            except (OSError, ValueError) as exc:
                sample["errors"][meta["name"]] = str(exc)
        sample["ended_at"] = now()
        sample["observations"] = {f"mcdu{side}": page_observations(sample["values"], side) for side in (1, 2)}
        report["samples"].append(sample)
        if index + 1 < args.samples:
            time.sleep(0.5)
    report["limitations"] = [
        "REST values are read sequentially, not as an atomic FMS snapshot.",
        "MCDU screen text changes with pages; absence on a page does not prove a plan field is unset.",
        "No page-independent FMS interface is claimed; no alternate-airport observation is inferred.",
        "Numeric flight-phase values are recorded without assuming their enum meanings.",
        "Candidate absence means absent in this session's registered directory, not impossible in every ToLiss version.",
    ]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"report": str(args.output), "registry_count": len(registry),
                      "present_fms_candidates": [n for n, v in candidates.items() if v["present"]],
                      "observations": [s["observations"] for s in report["samples"]],
                      "read_errors": [s["errors"] for s in report["samples"]]}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()

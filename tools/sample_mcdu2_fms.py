"""Exercise the production C++ FMS sampler against live MCDU2 screens.

Requires build/fms_probe.exe (scripts/build.ps1). Sends only the sampler's
allowlisted MCDU2 navigation commands, restores the recognized initial page,
and records screens and decisions. Does not change flight-plan entries.
The installed .xpl is not replaced or reloaded by this development tool.
"""
from __future__ import annotations
import argparse
import base64
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone, timedelta
import json
from pathlib import Path
import socket
import subprocess
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "AirbusFBW/MCDU2"
ALLOWED = {"DirTo", "Data", "Fpln", "Init", "SlewRight", "LSK4L", "FuelPred", "RadNav", "Menu", "SecFpln"}


def now():
    return datetime.now(timezone(timedelta(hours=8))).isoformat(timespec="milliseconds")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "reports/runtime/0.4.4/mcdu2-roundtrip.json")
    args = parser.parse_args()
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    base = "http://127.0.0.1:8086/api/v2"

    def get(path):
        with opener.open(base + path, timeout=5) as response:
            return json.load(response)

    refs = [d for d in get("/datarefs")["data"] if d["name"].startswith(PREFIX) and d["value_type"] == "data"]
    commands = {c["name"] for c in get("/commands")["data"]}
    report = {"started_at": now(), "driver": "build/fms_probe.exe using src/fms.cpp", "steps": [], "commands": []}

    def read_one(meta):
        raw = get(f'/datarefs/{meta["id"]}/value')["data"]
        value = base64.b64decode(raw).split(b"\0")[0].decode("utf-8", errors="replace")
        return meta["name"][len(PREFIX):], value

    def screen():
        # Bounded parallel GETs keep the diagnostic transport within the native
        # state machine's page timeout. The production plugin uses XPLM directly.
        with ThreadPoolExecutor(max_workers=12) as executor:
            return dict(executor.map(read_one, refs))

    def press(key):
        if key not in ALLOWED or PREFIX + key not in commands:
            raise RuntimeError("Unrecognized navigation command: " + key)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
            udp.sendto(b"CMND\0" + (PREFIX + key).encode("ascii") + b"\0", ("127.0.0.1", 49000))
        report["commands"].append({"at": now(), "command": PREFIX + key, "sent_once": True})

    driver = subprocess.Popen([str(ROOT / "build/fms_probe.exe")], stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, text=True, encoding="utf-8", bufsize=1)
    started = time.monotonic()
    last = None
    try:
        for index in range(30):
            fields = screen()
            elapsed = time.monotonic() - started
            driver.stdin.write(f"{elapsed} {len(fields)}\n")
            for key, value in fields.items():
                driver.stdin.write(key + "\t" + value.replace("\n", " ").replace("\r", " ") + "\n")
            driver.stdin.flush()
            response = driver.stdout.readline().rstrip("\r\n").split("\t")
            if len(response) != 10:
                raise RuntimeError("Invalid probe response: " + repr(response))
            key, status, observed, origin, dep, dest, arr, busy, saved, alternate = response
            last = {"command": key, "status": status, "observed": observed == "1", "origin": origin,
                    "departure_runway": dep, "destination": dest, "arrival_runway": arr, "alternate": alternate, "busy": busy == "1", "saved_page": saved}
            report["steps"].append({"at": now(), "elapsed": elapsed, "screen": fields, "decision": last})
            print(json.dumps(last, ensure_ascii=False), flush=True)
            if key:
                press(key)
            if index and not last["busy"]:
                report["completed"] = last["observed"]
                break
            if not index and not key:
                raise RuntimeError("Sampler deferred: " + status)
            time.sleep(.25)
        else:
            raise TimeoutError("MCDU2 transaction did not finish")
        if not report.get("completed"):
            raise RuntimeError("Sampling failed: " + last["status"])
    finally:
        try:
            if last and last["busy"]:
                # Best effort after an exception/Ctrl+C; never send an LSK
                # without confirming its navigation label on the current page.
                saved = last["saved_page"]
                restore = {"F-PLN": "Fpln", "DIR TO": "DirTo", "FUEL PRED": "FuelPred", "A/C STATUS": "Data",
                           "DATA INDEX": "Data", "INIT A": "Init", "INIT B": "Init", "RAD NAV": "RadNav",
                           "MCDU MENU": "Menu", "SEC INDEX": "SecFpln"}.get(saved)
                if restore:
                    press(restore)
                    time.sleep(.3)
                    restored = screen()
                    if saved == "A/C STATUS" and restored.get("cont4w", "").strip() == "<A/C STATUS":
                        press("LSK4L")
                        time.sleep(.3)
                        restored = screen()
                    elif saved == "INIT B" and any("FROM/TO" in v for v in restored.values()):
                        press("SlewRight")
                        time.sleep(.3)
                        restored = screen()
                    report["exception_restore_screen"] = restored
        finally:
            driver.stdin.close()
            driver.wait(timeout=5)
            report["ended_at"] = now()
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            print("Report:", args.output)


if __name__ == "__main__":
    main()

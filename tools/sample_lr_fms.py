"""Exercise the production LR row adapter and sampler using page keys only."""
import base64
import json
from pathlib import Path
import socket
import subprocess
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
KEYS = {"DirTo": "dir_intc", "Data": "data", "Fpln": "fpln",
        "FuelPred": "fuel_pred", "LSK4L": "ls_4l", "SlewRight": "next"}


def main():
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def get(path):
        with opener.open("http://127.0.0.1:8086/api/v2" + path, timeout=4) as response:
            return json.load(response)["data"]

    registry = {r["name"]: r for r in get("/datarefs")}
    refs = [registry[f"sim/cockpit2/radios/indicators/fms_cdu2_text_line{i}"] for i in range(16)]
    commands = {r["name"] for r in get("/commands")}
    report = {"steps": [], "commands": []}

    def screen():
        return [base64.b64decode(get(f"/datarefs/{r['id']}/value")).split(b"\0", 1)[0].decode("utf-8") for r in refs]

    def press(key):
        name = "sim/FMS2/" + KEYS[key]
        if name not in commands:
            raise RuntimeError("Missing command: " + name)
        for i in range(2 if key == "Fpln" else 1):
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
                udp.sendto(b"CMND\0" + name.encode("ascii") + b"\0", ("127.0.0.1", 49000))
            report["commands"].append({"at": time.monotonic(), "name": name})
            if i == 0 and key == "Fpln":
                time.sleep(.25)

    initial = screen()
    ident = any("ACTIVE NAV DATA BASE" in row for row in initial)
    if not (ident or "DATA INDEX" in initial[0]):
        raise RuntimeError("Probe requires MCDU2 A/C STATUS or DATA INDEX for verified restoration")
    driver = subprocess.Popen([str(ROOT / "build/lr_fms_probe.exe"), "--lr"], stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, text=True, encoding="utf-8")
    completed = False
    try:
        start = time.monotonic()
        for index in range(60):
            rows = screen()
            driver.stdin.write(f"{time.monotonic()-start} 16\n")
            for i, row in enumerate(rows):
                driver.stdin.write(str(i) + "\t" + row.replace("\n", " ").replace("\r", " ") + "\n")
            driver.stdin.flush()
            result = driver.stdout.readline().rstrip("\r\n").split("\t")
            if len(result) != 10:
                raise RuntimeError("Invalid sampler response: " + repr(result))
            key, status, observed, origin, dep, dest, arr, busy, saved, alternate = result
            decision = dict(status=status, observed=observed, origin=origin, departure_runway=dep,
                            destination=dest, arrival_runway=arr, alternate=alternate, busy=busy, saved_page=saved)
            report["steps"].append({"rows": rows, "decision": decision})
            if key:
                press(key)
            if index and busy == "0":
                completed = observed == "1"
                report["result"] = decision
                break
            time.sleep(.25)
        if not completed:
            raise RuntimeError("Sampler did not complete")
    finally:
        driver.stdin.close()
        driver.wait(timeout=5)
        if not completed:
            press("Data")
            time.sleep(.3)
            if ident and "<A/C STATUS" in screen()[8]:
                press("LSK4L")
                time.sleep(.3)
        report["final_rows"] = screen()
        report["restored"] = (any("ACTIVE NAV DATA BASE" in row for row in report["final_rows"]) if ident
                              else "DATA INDEX" in report["final_rows"][0])
        output = ROOT / "reports/runtime/lr-a333-20261102/fms-roundtrip.json"
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps({"report": str(output), "result": report.get("result"), "restored": report["restored"]}, ensure_ascii=False))
    if not report["restored"]:
        raise RuntimeError("Original page restoration failed")


if __name__ == "__main__":
    main()

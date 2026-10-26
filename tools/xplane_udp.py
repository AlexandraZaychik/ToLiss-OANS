"""Read X-Plane's native RREF UDP and invoke this plugin's existing commands.

Python standard library only. No bridge plugin, executable host, or custom
UDP server is required. Commands are sent once: several existing commands
toggle state and must not be retried automatically.
"""
from __future__ import annotations

import argparse
import json
import math
import socket
import struct
import time
from typing import Iterable


COMMANDS = (
    "window", "capt_nd", "fo_nd", "test_pattern", "diagnostics", "reload",
    "disconnect", "nearest", "capt_panel", "fo_panel", "capt_airport",
    "fo_airport", "capt_range_in", "capt_range_out", "fo_range_in",
    "fo_range_out", "capt_center", "fo_center", "test",
)
STATUS_REFS = (
    "sim/flightmodel/position/latitude",
    "sim/flightmodel/position/longitude",
    "sim/flightmodel/position/groundspeed",
    "sim/flightmodel/position/psi",
    "sim/time/paused",
    "AirbusFBW/NDrangeCapt", "AirbusFBW/NDrangeFO",
    "AirbusFBW/NDmodeCapt", "AirbusFBW/NDmodeFO",
    "AirbusFBW/CaptMAPAvail", "AirbusFBW/CoMAPAvail",
    "AirbusFBW/DUBrightness[1]", "AirbusFBW/DUBrightness[3]",
    "sim/cockpit2/electrical/bus_volts[0]",
    "sim/cockpit2/electrical/bus_volts[1]",
    "AirbusFBW/DisplayResolutionFallback",
    "AirbusFBW/PFDNDToggleCapt", "AirbusFBW/PFDNDToggleCo",
    "AirbusFBW/ECAMNDSwitching",
    "sim/graphics/view/view_type", "sim/graphics/VR/enabled",
)


class XPlaneUDP:
    def __init__(self, host: str = "127.0.0.1", port: int = 49000):
        self.peer = (socket.gethostbyname(host), port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("0.0.0.0", 0))
        self.next_index = 1

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def close(self):
        self.sock.close()

    def _subscribe(self, name: str, index: int, frequency: int):
        raw = name.encode("ascii")
        if len(raw) >= 400 or b"\0" in raw:
            raise ValueError("DataRef must be ASCII, without NUL, and under 400 bytes")
        self.sock.sendto(b"RREF\0" + struct.pack("<ii400s", frequency, index, raw), self.peer)

    def read(self, names: Iterable[str], timeout: float = 2.0) -> dict[str, float]:
        names = list(dict.fromkeys(names))
        if not names:
            return {}
        if timeout <= 0 or len(names) > 128:
            raise ValueError("timeout must be positive; at most 128 DataRefs per read")
        # Validate every name before creating subscriptions.
        for name in names:
            raw = name.encode("ascii")
            if len(raw) >= 400 or b"\0" in raw:
                raise ValueError("Invalid DataRef name")
        indexes = {}
        for name in names:
            indexes[self.next_index] = name
            self.next_index += 1
        values = {}
        deadline = time.monotonic() + timeout
        resend_at = 0.0
        try:
            while len(values) < len(names):
                now = time.monotonic()
                if now >= deadline:
                    break
                if now >= resend_at:
                    for index, name in indexes.items():
                        if name not in values:
                            self._subscribe(name, index, 10)
                    resend_at = now + 0.5
                self.sock.settimeout(min(0.2, deadline - now))
                try:
                    packet, sender = self.sock.recvfrom(65535)
                except socket.timeout:
                    continue
                # X-Plane receives at 49000 but replies from 49001 on this
                # installation. Correlate by host and outstanding index rather
                # than assuming the reply socket is also the request socket.
                if sender[0] != self.peer[0] or not packet.startswith(b"RREF,"):
                    continue
                if (len(packet) - 5) % 8:
                    continue
                for offset in range(5, len(packet), 8):
                    index, value = struct.unpack_from("<if", packet, offset)
                    if index in indexes and math.isfinite(value):
                        values[indexes[index]] = value
        finally:
            for index, name in indexes.items():
                self._subscribe(name, index, 0)
        missing = [name for name in names if name not in values]
        if missing:
            raise TimeoutError("No RREF reply for: " + ", ".join(missing))
        return values

    def command(self, name: str):
        if name.startswith("toliss_oans/"):
            name = name[len("toliss_oans/"):]
        if name not in COMMANDS:
            raise ValueError("Only the listed toliss_oans commands are supported")
        self.sock.sendto(b"CMND\0toliss_oans/" + name.encode("ascii") + b"\0", self.peer)


def aircraft_display_conditions(values: dict[str, float]) -> dict:
    """Aircraft inputs only; these do not prove actual drawing or plugin enablement."""
    result = {}
    for side, suffix, map_ref, brightness in (
        ("captain", "Capt", "CaptMAPAvail", 1),
        ("first_officer", "FO", "CoMAPAvail", 3),
    ):
        reasons = []
        if values[f"AirbusFBW/NDrange{suffix}"] != 0:
            reasons.append("physical ND range is not 10 NM")
        if values[f"AirbusFBW/NDmode{suffix}"] not in (2, 3, 4):
            reasons.append("ND mode is outside NAV/ARC/PLAN")
        if values[f"AirbusFBW/{map_ref}"] < 0.5:
            reasons.append("aircraft map unavailable")
        if values[f"AirbusFBW/DUBrightness[{brightness}]"] < 0.01:
            reasons.append("display brightness is off")
        swap = "PFDNDToggleCapt" if side == "captain" else "PFDNDToggleCo"
        if values[f"AirbusFBW/{swap}"] != 0 or values["AirbusFBW/ECAMNDSwitching"] != 0:
            reasons.append("display switching active")
        if max(values[f"sim/cockpit2/electrical/bus_volts[{i}]"] for i in (0, 1)) < 20:
            reasons.append("display bus voltage below 20 V")
        result[side] = {"aircraft_conditions_met": not reasons, "blocked_by": reasons}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=49000)
    parser.add_argument("--timeout", type=float, default=2.0)
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("status")
    get = sub.add_parser("get")
    get.add_argument("datarefs", nargs="+")
    command = sub.add_parser("command")
    command.add_argument("name", choices=COMMANDS)
    watch = sub.add_parser("watch")
    watch.add_argument("--hz", type=float, default=2)
    args = parser.parse_args()
    try:
        with XPlaneUDP(args.host, args.port) as client:
            if args.action == "command":
                client.command(args.name)
                print(json.dumps({"sent_once": f"toliss_oans/{args.name}", "acknowledged": False}))
                return
            if args.action == "watch" and not 0.1 <= args.hz <= 10:
                parser.error("--hz must be between 0.1 and 10")
            while True:
                started = time.monotonic()
                refs = args.datarefs if args.action == "get" else STATUS_REFS
                values = client.read(refs, args.timeout)
                result = {"time": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "values": values}
                if args.action != "get":
                    result["aircraft_display_conditions"] = aircraft_display_conditions(values)
                    result["scope"] = "Aircraft inputs only; check plugin log and ND visually for rendering."
                print(json.dumps(result, ensure_ascii=False), flush=True)
                if args.action != "watch":
                    break
                time.sleep(max(0, 1 / args.hz - (time.monotonic() - started)))
    except (OSError, ValueError, TimeoutError) as exc:
        parser.exit(1, f"{exc}\n")
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

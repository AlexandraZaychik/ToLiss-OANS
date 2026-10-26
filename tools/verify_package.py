"""Verify native exports, dependency boundary, default configuration and package manifest."""
import hashlib
import json
import re
from pathlib import Path
import pefile
import argparse

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--flavor', choices=('both', 'release', 'debug'), default='both')
selected = parser.parse_args().flavor

root = Path(__file__).resolve().parents[1]
version = re.search(r'project\(ToLissOANS VERSION ([0-9.]+)', (root / 'CMakeLists.txt').read_text(encoding='utf-8')).group(1)
source = (root / 'src/plugin.cpp').read_text(encoding='utf-8')
assert f'constexpr const char* version="{version}";' in source, 'Plugin version differs from CMake'
assert f'ToLissOANS/{version} airport-map' in source, 'HTTP User-Agent version differs from CMake'
def verify(package, flavor):
    binary = package / "64/win.xpl"
    data = binary.read_bytes()
    assert version.encode('ascii') in data, 'Built binary does not contain the release version'
    is_debug = flavor == "debug"
    assert (b"DataRef bind " in data) == is_debug, "Trace code/build flavor mismatch"
    pe = pefile.PE(str(binary))
    assert bool(pe.FILE_HEADER.Characteristics & 0x0002), "Requires executable PE image"
    section_names = [section.Name.decode(errors="replace").rstrip("\0") for section in pe.sections]
    has_symbols = any(name.startswith(".debug") or name.startswith("/") for name in section_names)
    assert has_symbols == is_debug, ("Debug symbols missing or release not stripped", section_names)
    exports = sorted(s.name.decode() for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name)
    expected = sorted(["XPluginStart", "XPluginStop", "XPluginEnable", "XPluginDisable", "XPluginReceiveMessage"])
    assert exports == expected, exports
    assert pe.FILE_HEADER.Machine == 0x8664, "Requires Windows x86-64"
    imports = sorted(e.dll.decode() for e in pe.DIRECTORY_ENTRY_IMPORT)
    allowed = {"kernel32.dll", "msvcrt.dll", "opengl32.dll", "glu32.dll", "gdi32.dll", "user32.dll", "winhttp.dll", "xplm_64.dll"}
    assert {s.lower() for s in imports} <= allowed, imports
    assert "experimental_write_brakes" not in (package / "config.ini").read_text(encoding="utf-8")
    profile = json.loads((package / "config/a20n.json").read_text(encoding="utf-8"))
    assert set(profile) == {"nd_brightness", "pfd_brightness", "nd_uv", "pfd_uv", "nd_corners"}
    assert not list(package.rglob("XPLM_64.dll")), "Test mock SDK must never ship"
    assert not list(package.rglob("*.exe")), "Offline test hosts must never ship with the aircraft plugin"
    assert sorted(p.relative_to(package).as_posix() for p in package.rglob('*.lua')) == ['assets/a350/map_engine.lua'], 'Only the extracted original A350 map engine is packaged'
    for required in ('assets/a350/LICENSE.txt', 'assets/a350/LUA_LICENSE.txt', 'assets/a350/provenance.json',
                     'assets/a350/A350_BTV_data/font_b612_px2.bin', 'assets/font_b612_OFL.txt'):
        assert (package / required).is_file(), f'Missing map runtime asset/license: {required}'
    manifest = {p.relative_to(package).as_posix(): {"bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                for p in sorted(package.rglob("*")) if p.is_file() and p.name != "manifest.json"}
    report = {"version": version, "map_renderer": "Original A350 BTV 2.14.3 Lua functions, private static Lua 5.4.9",
              "architecture": "Windows x86-64", "build_flavor": flavor,
              "detail_log": "config + detailed trace" if is_debug else "config only",
              "debug_symbols": has_symbols, "exports": exports, "imports": imports,
              "external_helper_required": False,
              "test_host_in_package": False, "xplane_started_by_verifier": False, "files": manifest}
    (package / "manifest.json").write_bytes(json.dumps(report, indent=2).encode("utf-8"))
    print(f"PASS {package.relative_to(root)} verification: {len(manifest)} files, {binary.stat().st_size} byte native XPL")

    return report

reports = {}
for flavor, directory in (("release", "dist/ToLissOANS"), ("debug", "dist/ToLissOANS-debug")):
    if selected != 'both' and flavor != selected:
        continue
    reports[flavor] = verify(root / directory, flavor)
for flavor, directory in (("release", "output/00_ToLissOANS"), ("debug", "output/00_ToLissOANS-debug")):
    if selected != 'both' and flavor != selected:
        continue
    package = root / directory
    if not package.exists():
        continue
    saved = json.loads((package / "manifest.json").read_text(encoding="utf-8-sig"))
    assert saved["version"] == version and saved["build_flavor"] == flavor
    actual = {p.relative_to(package).as_posix(): {"bytes": p.stat().st_size, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
              for p in sorted(package.rglob("*")) if p.is_file() and p.name != "manifest.json"}
    assert saved["files"] == actual, "Output manifest mismatch"
    assert not list(package.rglob("*.md")) and not list(package.rglob("reports")), "Unexpected output docs"
    assert not list(package.rglob("*.log")), "Runtime logs must not ship"
    binary = package / "64/win.xpl"
    assert hashlib.sha256(binary.read_bytes()).hexdigest() == reports[flavor]["files"]["64/win.xpl"]["sha256"]
    print(f"PASS {directory} manifest, content policy and binary hash")
(root / "reports/package_verification.json").write_bytes(json.dumps(reports, indent=2).encode("utf-8"))

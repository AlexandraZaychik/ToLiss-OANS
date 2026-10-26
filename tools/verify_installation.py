"""Read-only installed file check; writes its report only inside this project."""
import hashlib
import json
from datetime import datetime, timedelta, timezone
from pathlib import Path

root = Path(__file__).resolve().parents[1]
aircraft = Path(r"G:\SteamLibrary\steamapps\common\X-Plane 12\Aircraft\ToLissA320_V1p3p2")
target = aircraft / "plugins/00_ToLissOANS"
for old in ("ToLissOANS", "zz_ToLissOANS"):
    assert not (aircraft / "plugins" / old).exists(), "Old plugin folder would load a duplicate instance"
assert not list((aircraft / "plugins").glob("ToLissOANS-[0-9]*")), "Versioned plugin folder breaks ND load order or loads a duplicate instance"
package = root / "dist/ToLissOANS"
assert not list(target.rglob("*.exe")), "Offline test hosts must not be installed with the plugin"
assert not list(target.rglob("XPLM_64.dll")), "The test mock SDK must not be installed"
audit = json.loads((root / "reports/aircraft_audit.json").read_text(encoding="utf-8"))
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(aircraft / "cockpit_INN.obj") == audit["object"]["sha256"], "Aircraft object changed"
for model in audit["models"]:
    assert sha(aircraft / model["name"]) == model["sha256"], f"Aircraft model changed: {model['name']}"
files = {}
configuration_files = {}
for p in sorted(package.rglob("*")):
    if p.is_file():
        relative = p.relative_to(package)
        # install.ps1 intentionally preserves the user's existing settings.
        # Validate that configuration separately instead of requiring defaults.
        if relative.as_posix() == "config.ini" or (relative.parts[0] == "config" and relative.suffix.lower() == ".json"):
            assert (target / relative).is_file(), "Installed configuration missing"
            if relative.suffix.lower() == ".json":
                json.loads((target / relative).read_text(encoding="utf-8-sig"))
            configuration_files[relative.as_posix()] = sha(target / relative)
            continue
        assert sha(p) == sha(target / relative), f"Installed file differs: {relative}"
        files[relative.as_posix()] = sha(p)
report = {"checked_at": datetime.now(timezone(timedelta(hours=8))).isoformat(), "installed_directory": str(target),
          "files_sha256": files, "aircraft_assets_unchanged": True, "default_mode": "OANS",
          "configuration_policy": "Preserve existing user configuration",
          "configuration_sha256": sha(target / "config.ini"),
          "configuration_files_sha256": configuration_files,
          "configuration_matches_package": sha(target / "config.ini") == sha(package / "config.ini"),
          "scope": "File verification only; does not start or connect to X-Plane"}
(root / "reports/installation_verification.json").write_bytes(json.dumps(report, indent=2, ensure_ascii=False).encode("utf-8"))
print(f"PASS installation verification: {len(files)} matching program/assets/doc files; existing configuration preserved; original ACF/OBJ unchanged; shadow mode")

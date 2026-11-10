"""Count expanded scenarios and verify execution evidence (stdlib only)."""
import argparse
import json
from pathlib import Path
import re
from collections import Counter

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--logs", nargs="*", type=Path)
args = parser.parse_args()
manifest = json.loads((root / "tests/scenarios.json").read_text(encoding="utf-8"))
expected = [row["id"] for row in manifest]
if len(expected) != len(set(expected)) or len(expected) > 100:
    raise SystemExit("Duplicate scenario IDs or test budget exceeded")
print(f"Independent scenarios: {len(expected)} / 100")
for module, count in sorted(Counter(row["module"] for row in manifest).items()):
    print(f"  {module}: {count}")
if args.logs is not None:
    observed = []
    for path in args.logs:
        text = path.read_text(encoding="utf-8-sig", errors="replace")
        observed += re.findall(r"^(?:\d+: )?CASE ([^\r\n]+)", text, re.MULTILINE)
    missing = set(expected) - set(observed)
    extra = set(observed) - set(expected)
    if missing or extra:
        raise SystemExit(f"Missing scenarios: {sorted(missing)}; unregistered scenarios: {sorted(extra)}")
    print(f"PASS: all {len(expected)} scenarios executed; {len(observed)} executions including repeats")

"""Capture fresh draw-context ND reports and native simulator screenshots.

Keep aircraft, camera and pointer still while collecting. No camera/settings
are written. Each diagnostic command is correlated by the report's mtime.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys
import time

from xplane_udp import XPlaneUDP


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--samples', type=int, default=120)
    parser.add_argument('--interval', type=float, default=.05)
    parser.add_argument('--screenshots', type=int, default=6)
    parser.add_argument('--plugin', type=Path, default=Path(
        r'G:\SteamLibrary\steamapps\common\X-Plane 12\Aircraft\ToLissA320_V1p3p2\plugins\00_ToLissOANS'))
    args = parser.parse_args()
    if args.samples < 1 or args.interval < 0 or not 0 <= args.screenshots <= args.samples:
        parser.error('invalid sample count, interval or screenshot count')
    args.output.mkdir(parents=True, exist_ok=True)
    report = args.plugin / 'interfaces.txt'
    shots = {round(i*(args.samples-1)/max(1, args.screenshots-1))
             for i in range(args.screenshots)}
    samples = []
    with XPlaneUDP() as client:
        for i in range(args.samples):
            previous = report.stat().st_mtime_ns if report.exists() else None
            client.command('diagnostics')
            deadline = time.monotonic()+3
            while True:
                if report.exists() and report.stat().st_mtime_ns != previous:
                    text = report.read_text(encoding='utf-8')
                    if 'click_context ' in text and 'view=2 ' in text:
                        break
                if time.monotonic() > deadline:
                    raise TimeoutError('No complete, fresh diagnostic report')
                time.sleep(.005)
            (args.output / f'{i:03d}-interfaces.txt').write_text(text, encoding='utf-8')
            projection = next(l for l in text.splitlines() if l.startswith('click_projection '))
            context = next(l for l in text.splitlines() if l.startswith('click_context '))
            mapped = re.search(r'mapped=1 ([\d.eE+-]+),([\d.eE+-]+)', context)
            sample = {'index': i, 'wall_time': time.time(), 'projection': projection,
                      'context': context, 'raw': list(map(int, re.search(r'raw_xy=(\d+),(\d+)', context).groups())),
                      'mapped': list(map(float, mapped.groups())) if mapped else None}
            samples.append(sample)
            if i in shots:
                subprocess.run([sys.executable, str(Path(__file__).with_name('capture_xplane.py')),
                                str(args.output / f'{i:03d}.png')], check=True)
            time.sleep(args.interval)
    points = [s['mapped'] for s in samples if s['mapped'] is not None]
    result = {'scope': 'Stationary pointer/camera sample; reports precede screenshots, not frame-synchronous',
              'samples': samples, 'valid_cursor_samples': len(points),
              'raw_positions': sorted({tuple(s['raw']) for s in samples}),
              'mapped_peak_to_peak': [max(p[j] for p in points)-min(p[j] for p in points)
                                      for j in range(2)] if points else None}
    (args.output / 'samples.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k != 'samples'}, indent=2))


if __name__ == '__main__':
    main()

"""Read-only TCAS panel snapshots and change recording from the local X-Plane API."""
import argparse
import base64
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import re
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'reports/runtime/tcas-20261028'
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def get(path):
    with opener.open('http://127.0.0.1:8086/api/v2' + path, timeout=3) as response:
        return json.load(response)['data']


def now():
    return datetime.now(timezone(timedelta(hours=8))).isoformat(timespec='milliseconds')


def record(path, event):
    with path.open('a', encoding='utf-8') as stream:
        stream.write(json.dumps(event, ensure_ascii=False) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', default='unlabelled')
    parser.add_argument('--watch', action='store_true')
    parser.add_argument('--seconds', type=float, default=7200)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    registry = get('/datarefs')
    selected = [r for r in registry if (
        re.search(r'^AirbusFBW/(?:XPDR|TCAS)', r['name'])
        or r['name'].startswith('ckpt/transponder/')
        or re.search(r'^sim/cockpit2/radios/(?:actuators|indicators)/(?:transponder|tcas)', r['name'])
        or r['name'] in ('sim/cockpit/radios/transponder_mode',
                         'sim/cockpit2/EFIS/EFIS_tcas_on',
                         'sim/cockpit2/EFIS/EFIS_tcas_on_copilot'))]
    (OUT / 'registry.json').write_text(json.dumps(selected, indent=2), encoding='utf-8')
    previous = {}
    start = time.monotonic()
    while True:
        started = now()
        values, errors = {}, {}
        for ref in selected:
            try:
                value = get(f"/datarefs/{ref['id']}/value")
                if ref['value_type'] == 'data':
                    value = {'raw_base64': value, 'text': base64.b64decode(value).split(b'\0', 1)[0].decode('utf-8', errors='replace')}
                values[ref['name']] = value
            except Exception as error:
                errors[ref['name']] = str(error)
        changes = {name: {'before': previous.get(name), 'after': value}
                   for name, value in values.items() if name not in previous or previous[name] != value}
        event = {'started_at': started, 'finished_at': now(), 'label': args.label,
                 'kind': 'baseline' if not previous else 'change', 'changes': changes}
        if errors:
            event['errors'] = errors
        if changes or errors:
            record(OUT / ('changes.jsonl' if args.watch else 'snapshots.jsonl'), event)
            print(json.dumps(event, ensure_ascii=False), flush=True)
        if not previous or not args.watch:
            (OUT / ('watch-baseline.json' if args.watch else 'latest-snapshot.json')).write_text(
                json.dumps({'started_at': started, 'finished_at': now(), 'label': args.label,
                            'values': values, 'errors': errors}, indent=2, ensure_ascii=False), encoding='utf-8')
        previous.update(values)
        if not args.watch or time.monotonic() - start >= args.seconds:
            break
        time.sleep(.25)


if __name__ == '__main__':
    main()

"""Read-only Zibo FMS discovery/snapshot through the local X-Plane Web API."""
import argparse
import base64
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone, timedelta
import json
from pathlib import Path
import re
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
FIELDS = dict(origin='ref_icao', departure_runway='ref_runway',
              destination='dest_icao', arrival_runway='dest_runway')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', default='http://127.0.0.1:8086/api/v2')
    parser.add_argument('--output', type=Path, default=ROOT/'reports/runtime/zibo-b738-20261102/fms-probe.json')
    args = parser.parse_args()
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def get(path):
        with opener.open(args.base + path, timeout=5) as response:
            return json.load(response)['data']

    registry = get('/datarefs')
    selected_names = {'laminar/B738/fms/' + value for value in FIELDS.values()}
    selected_names.update('laminar/B738/fms/' + name for name in
                          ('legs', 'legs_ext', 'legs_lat', 'legs_lon', 'legs_type',
                           'rte12_act', 'legs_mod_active', 'ref_runway_icao', 'dest_runway_icao'))
    selected_names.add('laminar/B738/vnav/legs_num')
    selected_names.update(('sim/aircraft/view/acf_ICAO', 'sim/aircraft/view/acf_ui_name', 'laminar/B738/release'))
    refs = [r for r in registry if r['name'] in selected_names or
            r['name'].startswith('laminar/B738/fmc') and r['value_type'] == 'data' or
            r['name'].startswith('sim/cockpit2/radios/indicators/fms_cdu') and '_text_line' in r['name'] or
            r['name'].startswith('laminar/B738/') and re.search('altern|altn', r['name'], re.I)]

    def read(meta):
        value = get(f"/datarefs/{meta['id']}/value")
        if meta['value_type'] == 'data':
            value = base64.b64decode(value).split(b'\0', 1)[0].decode('utf-8', errors='replace')
        return dict(meta, value=value)

    samples = []
    for _ in range(3):
        with ThreadPoolExecutor(max_workers=8) as executor:
            values = list(executor.map(read, refs))
        by_name = {r['name']: r['value'] for r in values}
        result = {field: by_name.get('laminar/B738/fms/' + suffix) for field, suffix in FIELDS.items()}
        samples.append(dict(at=datetime.now(timezone(timedelta(hours=8))).isoformat(),
                            result=result, values=values))
        if len(samples) < 3:
            time.sleep(.5)
    report = dict(read_only=True, commands_sent=[], samples=samples,
                  alternate_named_refs=[r for r in registry if re.search('altern|altn', r['name'], re.I)])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(dict(report=str(args.output), result=samples[-1]['result'],
                          endpoint_samples_stable=all(s['result'] == samples[0]['result'] for s in samples),
                          vhhh_matches=[dict(name=r['name'], value=r['value']) for r in samples[-1]['values']
                                        if isinstance(r['value'], str) and 'VHHH' in r['value']]), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()

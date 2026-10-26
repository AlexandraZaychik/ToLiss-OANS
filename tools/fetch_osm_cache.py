"""Fetch A350-compatible airport building data with its original Overpass query.

One explicit invocation per airport; no simulator restart or plugin mutation.
The original A350 osm_build() performs all building simplification/rendering.
"""
import argparse
import json
import re
import urllib.parse
import urllib.request
from pathlib import Path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('icao')
    parser.add_argument('--output',type=Path,default=Path('cache'))
    args=parser.parse_args()
    icao=args.icao.upper()
    if not re.fullmatch(r'[A-Z0-9]{3,8}',icao):
        parser.error('Invalid airport code')
    # A350_BTV.lua:2350-2352, same area/building/terminal query.
    query=(f'[out:json][timeout:120];area["aeroway"="aerodrome"]["icao"="{icao}"]->.a;'
           '(way["building"](area.a);way["aeroway"="terminal"](area.a);)->.w;'
           '(rel["building"](area.a);rel["aeroway"="terminal"](area.a);)->.r;'
           '.w out tags geom;.r out body geom;')
    body=urllib.parse.urlencode({'data':query}).encode()
    for url in ('https://overpass-api.de/api/interpreter','https://overpass.private.coffee/api/interpreter'):
        try:
            request=urllib.request.Request(url,data=body,headers={'User-Agent':'ToLissOANS/0.4.0 airport-map'})
            with urllib.request.urlopen(request,timeout=150) as response:
                data=response.read()
            parsed=json.loads(data)
            if not isinstance(parsed.get('elements'),list) or not parsed['elements']:
                raise ValueError('No building elements returned')
            args.output.mkdir(parents=True,exist_ok=True)
            path=args.output/f'osm2_{icao}.json'
            path.write_bytes(data)
            print(f'{path}: {len(parsed["elements"])} OSM elements, {len(data)} bytes, source={url}',flush=True)
            return
        except Exception as error:
            print(f'{url}: {error}',flush=True)
    raise SystemExit('No valid OSM cache downloaded')


if __name__=='__main__':
    main()

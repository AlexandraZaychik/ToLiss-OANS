"""Extract the inspected LR A333 panel geometry without modifying aircraft assets."""
from __future__ import annotations
import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import struct

PROJECT = Path(__file__).resolve().parents[1]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def panel_quads(obj):
    """Find static, panel-textured quads by triangle connectivity, not draw order."""
    vertices, indices, quads = [], [], []
    panel, depth = False, 0
    for line_no, line in enumerate(obj.read_text(encoding='utf-8').splitlines(), 1):
        t = line.split()
        if not t:
            continue
        tag = t[0]
        if tag == 'VT':
            values = tuple(map(float, t[1:]))
            require(len(values) == 8 and all(map(math.isfinite, values)), 'Invalid OBJ vertex')
            vertices.append(values[:3] + values[6:8])
        elif tag in ('IDX', 'IDX10'):
            indices.extend(map(int, t[1:]))
        elif tag == 'ANIM_begin':
            depth += 1
        elif tag == 'ANIM_end':
            depth -= 1
            require(depth >= 0, 'Unbalanced OBJ animation')
        elif tag in ('ATTR_cockpit', 'ATTR_cockpit_lit_only'):
            panel = True
        elif tag in ('ATTR_cockpit_region', 'ATTR_cockpit_device'):
            raise ValueError('Panel region/device layout needs a separate extraction rule')
        elif tag == 'ATTR_no_cockpit':
            panel = False
        elif tag == 'TRIS' and panel and depth == 0:
            first, count = map(int, t[1:])
            require(first >= 0 and count % 3 == 0 and first + count <= len(indices), 'Invalid triangle range')
            selected = indices[first:first + count]
            require(all(0 <= i < len(vertices) for i in selected), 'Invalid vertex index')
            triangles = [tuple(vertices[i] for i in selected[j:j + 3]) for j in range(0, count, 3)]
            parents = {v: v for tri in triangles for v in tri}

            def find(v):
                while parents[v] != v:
                    parents[v] = parents[parents[v]]
                    v = parents[v]
                return v

            for tri in triangles:
                for v in tri[1:]:
                    parents[find(v)] = find(tri[0])
            groups = defaultdict(list)
            for tri in triangles:
                groups[find(tri[0])].append(tri)
            for group in groups.values():
                points = set(v for tri in group for v in tri)
                if len(group) != 2 or len(points) != 4:
                    continue
                points = sorted(points, key=lambda p: (-p[4], p[3]))
                corners = [points[0], points[1], points[3], points[2]]
                uv = [min(p[3] for p in points), min(p[4] for p in points),
                      max(p[3] for p in points), max(p[4] for p in points)]
                expected = [(uv[0], uv[3]), (uv[2], uv[3]), (uv[2], uv[1]), (uv[0], uv[1])]
                require(all(abs(p[3 + j] - e[j]) < 1e-6 for p, e in zip(corners, expected) for j in range(2)),
                        'Panel quad UV is not an axis-aligned rectangle')
                quads.append({'line': line_no, 'uv': uv, 'obj_corners': [list(p[:3]) for p in corners]})
    require(depth == 0, 'Unbalanced OBJ animation')
    return quads


def extract(model):
    acf_text = model.read_text(encoding='utf-8')
    props = {}
    for line in acf_text.splitlines():
        if line.startswith('P '):
            parts = line.split(None, 2)
            if len(parts) == 3:
                props[parts[1]] = parts[2].strip()
    require(props.get('acf/_studio') == 'Laminar Research', 'ACF studio must be Laminar Research')
    require(props.get('acf/_ICAO') == 'A333', 'This extractor supports the inspected LR A333 layout only')
    attachments = [key[:-len('/_v10_att_file_stl')] for key, value in props.items()
                   if key.startswith('_obja/') and key.endswith('/_v10_att_file_stl')
                   and Path(value).stem.lower() == 'a330_cockpit']
    require(len(attachments) == 1, 'Expected one interior cockpit attachment')
    attachment = attachments[0]
    require(all(float(props[attachment + f'/_v10_att_{axis}_ref']) == 0 for axis in ('phi', 'psi', 'the')),
            'Rotated cockpit attachment needs a separate transform')
    relative = Path(props[attachment + '/_v10_att_file_stl'])
    require(not relative.is_absolute(), 'Cockpit attachment must be relative')
    obj = (model.parent / 'objects' / relative).resolve()
    offset = [float(props[attachment + f'/_v10_att_{axis}_acf_prt_ref']) * .3048 for axis in 'xyz']
    offset[1] -= float(props['acf/_cgY']) * .3048
    offset[2] -= float(props['acf/_cgZ']) * .3048
    panel = model.parent / 'cockpit_3d/-PANELS-/Panel.png'
    header = panel.read_bytes()[:24]
    require(header[:8] == b'\x89PNG\r\n\x1a\n', 'Invalid panel PNG')
    width, height = struct.unpack('>II', header[16:24])
    require((width, height) == (2048, 1024), 'Unexpected LR A333 panel dimensions')
    quads = panel_quads(obj)
    obj_text = obj.read_text(encoding='utf-8')
    screens = {}
    for kind in ('ND', 'PFD'):
        for side, manip_label in (('Capt', 'Captain'), ('FO', 'F/O')):
            # These named ACF instruments locate each 512x512 display tile and
            # explicitly declare its brightness channel. Cross-check the OBJ knob.
            label = f'Blue Glow {kind} {side}'
            match = re.search(r'^\s*gen_rotary ' + label + r'\s*\n\s*POS ([\d.]+) ([\d.]+)\n'
                              r'(?P<body>(?:[ \t]+[^\n]*\n|\n)*?)(?=  \S|PANEL_3D_END)', acf_text, re.M)
            require(match is not None, f'Missing ACF display anchor: {label}')
            x, y = map(float, match.group(1, 2))
            brightness = re.search(r'LIGHT_RHEOSTAT (\d+)', match['body'])
            require(brightness is not None, f'Missing ACF brightness: {label}')
            brightness = int(brightness[1])
            knob = re.search(r'ATTR_manip_axis_knob[^\n]*instrument_brightness_ratio\[(\d+)\]\s+'
                             + re.escape(f'{manip_label} {kind} Brightness') + r'\s*$', obj_text, re.M)
            require(knob is not None and int(knob[1]) == brightness, f'ACF/OBJ brightness mismatch: {label}')
            # The ACF glow anchor is at the tile's lower horizontal midpoint.
            candidates = [q for q in quads if
                          (x - 256) / width <= q['uv'][0] < q['uv'][2] <= (x + 256) / width
                          and y / height <= q['uv'][1] < q['uv'][3] <= (y + 512) / height
                          and (q['uv'][2] - q['uv'][0]) * width > 480
                          and (q['uv'][3] - q['uv'][1]) * height > 480]
            require(len(candidates) == 1, f'Expected one static display quad: {label}')
            screen = dict(candidates[0], brightness=brightness, panel_anchor=[x, y])
            screen['cg_corners'] = [[p[i] + offset[i] for i in range(3)] for p in screen['obj_corners']]
            screens[f'{kind.lower()}_{side.lower()}'] = screen
    profile = {}
    for kind in ('nd', 'pfd'):
        selected = [screens[f'{kind}_{side}'] for side in ('capt', 'fo')]
        profile[f'{kind}_brightness'] = [s['brightness'] for s in selected]
        # LR has one panel layout; both slots in the shared schema are identical.
        profile[f'{kind}_uv'] = [[s['uv'] for s in selected] for _ in range(2)]
    profile['nd_corners'] = [screens[f'nd_{side}']['cg_corners'] for side in ('capt', 'fo')]
    hashes = {label: hashlib.sha256(path.read_bytes()).hexdigest() for label, path in (('acf', model), ('obj', obj))}
    report = {'family': 'lr', 'studio': props['acf/_studio'], 'icao': props['acf/_ICAO'],
              'model': str(model), 'object': str(obj), 'sha256': hashes, 'attachment': attachment,
              'panel_size': [width, height], 'object_to_cg': offset, 'screens': screens,
              'runtime_display_verified': False,
              'note': 'One LR texture layout, duplicated in the shared normal/fallback slots. Brightness indices refer to sim/cockpit2/switches/instrument_brightness_ratio.'}
    return profile, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=PROJECT / 'config/lr/a333.json')
    parser.add_argument('--report', type=Path, default=PROJECT / 'reports/LR_A333_PROFILE_20261030.json')
    args = parser.parse_args()
    profile, report = extract(args.model.resolve())
    for path, data in ((args.output, profile), (args.report, report)):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    print(json.dumps({'profile': str(args.output), 'report': str(args.report),
                      'family': report['family'], 'nd_brightness': profile['nd_brightness'],
                      'pfd_brightness': profile['pfd_brightness'], 'nd_uv': profile['nd_uv'][0]}, indent=2))


if __name__ == '__main__':
    main()

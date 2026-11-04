"""Extract Zibo B738 normal ND/PFD quads; never modifies aircraft assets."""
from pathlib import Path
import argparse
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[1]


def extract(model):
    props = {}
    for line in model.read_text(encoding='utf-8').splitlines():
        t = line.split(None, 2)
        if len(t) == 3 and t[0] == 'P':
            props[t[1]] = t[2].strip()
    assert props['acf/_studio'] == 'Laminar Research modified by Zibo and Twkster'
    assert props['acf/_ICAO'] == 'B738'
    attachment = '_obja/0'
    obj = (model.parent / 'objects' / props[attachment + '/_v10_att_file_stl']).resolve()
    assert obj.name == 'b738_cockpit.obj'
    assert all(float(props[attachment + f'/_v10_att_{a}_ref']) == 0 for a in ('phi', 'psi', 'the'))
    offset = [float(props[attachment + f'/_v10_att_{a}_acf_prt_ref']) * .3048 for a in 'xyz']
    offset[1] -= float(props['acf/_cgY']) * .3048
    offset[2] -= float(props['acf/_cgZ']) * .3048
    panel = model.parent / 'cockpit_3d/-PANELS-/Panel.png'
    size = struct.unpack('>II', panel.read_bytes()[16:24])
    assert size == (2048, 2048), size
    vertices, indices, stack, screens = [], [], [], {}
    cockpit = False
    for n, line in enumerate(obj.read_text(encoding='utf-8').splitlines(), 1):
        t = line.split()
        if not t:
            continue
        tag = t[0]
        if tag == 'VT':
            vertices.append(list(map(float, t[1:])))
        elif tag in ('IDX', 'IDX10'):
            indices.extend(map(int, t[1:]))
        elif tag == 'ANIM_begin':
            stack.append([])
        elif tag == 'ANIM_end':
            stack.pop()
        elif tag.startswith('ANIM_') and stack:
            stack[-1].append(t)
        elif tag == 'ATTR_cockpit':
            cockpit = True
        elif tag == 'ATTR_no_cockpit':
            cockpit = False
        elif tag == 'TRIS' and cockpit and int(t[2]) == 6:
            animations = [a for group in stack for a in group]
            if any(a[0] != 'ANIM_hide' for a in animations):
                continue
            for side, name in enumerate(('cpt', 'fo')):
                for kind, unit in (('nd', 'idu'), ('pfd', 'odu')):
                    ref = f'laminar/B738/display_units/{unit}_{name}'
                    hides = [a for a in animations if a[-1] == ref]
                    normal = [['ANIM_hide', '-1.000000', '-0.500000', ref]]
                    if kind == 'nd':
                        normal.append(['ANIM_hide', '0.500000', '3.500000', ref])
                    if hides != normal:
                        continue
                    points = [vertices[i] for i in set(indices[int(t[1]):int(t[1])+6])]
                    assert len(points) == 4
                    points.sort(key=lambda p: (-p[7], p[6]))
                    points = [points[0], points[1], points[3], points[2]]
                    uv = [min(p[6] for p in points), min(p[7] for p in points),
                          max(p[6] for p in points), max(p[7] for p in points)]
                    assert .23 < uv[2]-uv[0] < .25 and .23 < uv[3]-uv[1] < .25
                    key = f'{kind}{side}'
                    assert key not in screens, key
                    screens[key] = dict(line=n, uv=uv, hide=normal,
                                        corners=[[p[a]+offset[a] for a in range(3)] for p in points])
    assert not stack and len(screens) == 4
    profile = {'nd_brightness': [2, 3], 'pfd_brightness': [0, 1]}
    for kind in ('nd', 'pfd'):
        profile[kind + '_uv'] = [[screens[f'{kind}{side}']['uv'] for side in range(2)] for _ in range(2)]
    profile['nd_corners'] = [screens[f'nd{side}']['corners'] for side in range(2)]
    report = dict(model=str(model), object=str(obj), panel_size=size, object_to_cg=offset,
                  screens=screens, physical_oans_range_nm=5, runtime_display_verified=False,
                  sha256={name: hashlib.sha256(p.read_bytes()).hexdigest()
                          for name, p in [('acf', model), ('obj', obj), ('panel', panel)]})
    return profile, report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'config/zibo/b738.json')
    parser.add_argument('--report', type=Path, default=ROOT / 'reports/ZIBO_B738_PROFILE_20261102.json')
    args = parser.parse_args()
    profile, report = extract(args.model.resolve())
    for path, data in ((args.output, profile), (args.report, report)):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(profile=str(args.output), panel_size=report['panel_size'],
                          nd_uv=profile['nd_uv'][0]), indent=2))

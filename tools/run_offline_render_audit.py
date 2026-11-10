"""Offline-only experiment: builds isolated probes, never the simulator plugin."""
from pathlib import Path
import subprocess, argparse, json, os
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--compile-only',action='store_true')
    args=parser.parse_args()
    work=ROOT/'build/render-audit-34';work.mkdir(parents=True,exist_ok=True)
    out=ROOT/'reports/runtime/render-audit-34';out.mkdir(parents=True,exist_ok=True)
    compiler=Path(r'D:\Qt\Qt6\Tools\mingw1310_64\bin\g++.exe')
    env=os.environ.copy();env['PATH']=str(compiler.parent)+os.pathsep+env['PATH']
    original=(ROOT/'src/a350_map.cpp').read_text(encoding='utf-8')
    lean=original
    replacements={
        '    p.points(view.crosses);p.points(view.flags);': '    if(layer>=2){lua_pushnil(L);lua_pushnil(L);}else {p.points(view.crosses);p.points(view.flags);}',
        '    p.string(view.fms_airport);p.string(view.fms_runway);p.alerts(view.runway_ahead);lua_pushboolean(L,view.alert_blink);': '    if(layer>=2){lua_pushnil(L);lua_pushnil(L);}else {p.string(view.fms_airport);p.string(view.fms_runway);}\n    if(layer>=2&&layer!=5)lua_pushnil(L);else p.alerts(view.runway_ahead);lua_pushboolean(L,view.alert_blink);',
        '    lua_pushboolean(L,view.outside_plan);p.string(view.map_runway);p.traffic(view.traffic);': '    lua_pushboolean(L,view.outside_plan);if(layer>=2)lua_pushliteral(L, "");else p.string(view.map_runway);\n    if(layer==3||layer==5)lua_pushnil(L);else p.traffic(view.traffic);',
        'lua_settop(L,0);cache.view=view;cache.ready=true;cache.layer=layer;':'lua_settop(L,0);if(layer<2)cache.view=view;cache.ready=true;cache.layer=layer;'
    }
    for a,b in replacements.items():
        assert lean.count(a)==1,a
        lean=lean.replace(a,b)
    (work/'a350_map_lean.cpp').write_text(lean,encoding='utf-8')
    for variant,source in [('baseline',ROOT/'src/a350_map.cpp'),('lean',work/'a350_map_lean.cpp')]:
        exe=work/(variant+'.exe')
        cmd=[str(compiler),'-std=c++17','-O2','-DOANS_DEBUG=0','-DNOMINMAX','-static','-Isrc','-Ivendor/lua-5.4.9/src','src/core.cpp','src/runway_ahead.cpp',str(source),'tools/offline_render_audit.cpp','build/lua/liblua.a','-lopengl32','-lgdi32','-luser32','-o',str(exe)]
        print('Building offline probe',variant,flush=True)
        subprocess.run(cmd,cwd=ROOT,env=env,check=True)
    if args.compile_only:return
    xplane=r'G:\SteamLibrary\steamapps\common\X-Plane 12'
    buildings={
        'ZUCK':ROOT/'reports/runtime/detail-quality-ZUCK-21R-02L/buildings.json/dsf_buildings_ZUCK.json',
        'ZGGG':ROOT/'reports/runtime/detail-quality-ZGGG/cache/dsf_buildings_ZGGG.json',
        'ZBAA':out/'dsf_buildings_ZBAA.json/dsf_buildings_ZBAA.json'}
    if not buildings['ZBAA'].is_file():
        p=subprocess.run([str(ROOT/'build/dsf_buildings_probe.exe'),xplane,'ZBAA',str(buildings['ZBAA'].parent)],cwd=ROOT,capture_output=True)
        (out/'ZBAA-extract.log').write_bytes(p.stdout+p.stderr);p.check_returncode()
    assert all(path.is_file() for path in buildings.values()),buildings
    for icao,path in buildings.items():
        for variant in ['baseline','lean']:
            print('Running',icao,variant,flush=True)
            p=subprocess.run([str(work/(variant+'.exe')),str(ROOT/'assets'),xplane,icao,str(path),str(out),variant],cwd=ROOT,capture_output=True)
            (out/(icao+'-'+variant+'.csv')).write_bytes(p.stdout)
            (out/(icao+'-'+variant+'.err')).write_bytes(p.stderr)
            p.check_returncode()
            meta=p.stdout.decode('utf-8').splitlines()[0].split(',')
            assert int(meta[-2])+int(meta[-1])>0, 'Buildings were not loaded: '+icao
    print('Offline audit finished:',out,flush=True)
if __name__=='__main__':main()

"""Compile/run isolated GL regressions and airport probes; never build the plugin."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    compiler = Path(r'D:\Qt\Qt6\Tools\mingw1310_64\bin\g++.exe')
    work = ROOT / 'build/batch-submission'
    out = ROOT / 'reports/runtime/batch-submission'
    work.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env['PATH'] = str(compiler.parent) + os.pathsep + env['PATH']
    flags = [str(compiler), '-std=c++17', '-O2', '-DNOMINMAX', '-DOANS_DEBUG=0', '-static', '-Isrc']
    def run(cmd, name):
        p = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True)
        (out / (name + '.log')).write_bytes(p.stdout + p.stderr)
        print((p.stdout + p.stderr).decode('utf-8', errors='replace'), flush=True)
        p.check_returncode()
    run(flags + ['tests/gl_state_tests.cpp', '-lopengl32', '-lglu32', '-lgdi32', '-o', str(work / 'gl_state_tests.exe')], 'compile-state')
    run([str(work / 'gl_state_tests.exe')], 'state-tests')
    run(flags + ['-Ivendor/lua-5.4.9/src', 'src/core.cpp', 'src/runway_ahead.cpp', 'src/a350_map.cpp',
                 'tools/verify_batch_submission.cpp', 'build/lua/liblua.a', '-lopengl32', '-lgdi32', '-luser32',
                 '-o', str(work / 'verify_batch_submission.exe')], 'compile-airport')
    buildings = {
        'ZUCK': ROOT / 'reports/runtime/detail-quality-ZUCK-21R-02L/buildings.json/dsf_buildings_ZUCK.json',
        'ZGGG': ROOT / 'reports/runtime/detail-quality-ZGGG/cache/dsf_buildings_ZGGG.json',
        'ZBAA': ROOT / 'reports/runtime/render-audit-34/dsf_buildings_ZBAA.json/dsf_buildings_ZBAA.json',
    }
    for airport, path in buildings.items():
        if not path.is_file():
            raise FileNotFoundError(path)
        run([str(work / 'verify_batch_submission.exe'), str(ROOT / 'assets'),
             r'G:\SteamLibrary\steamapps\common\X-Plane 12', airport, str(path), str(out / (airport + '.csv'))], airport)

if __name__ == '__main__':
    main()

"""Test the actual callback body in isolation; never build a simulator plugin."""
from pathlib import Path
import subprocess, os
root=Path(__file__).resolve().parents[1]
work=root/'build/gauge-draw-checks';work.mkdir(parents=True,exist_ok=True)
out=root/'reports/runtime/gauge-draw-checks';out.mkdir(parents=True,exist_ok=True)
source=(root/'src/plugin.cpp').read_text(encoding='utf-8')
a=source.index('struct PanelTrace {');b=source.index('\n};',a)+3
(work/'gauge_trace_under_test.hpp').write_text(source[a:b],encoding='utf-8')
a=source.index('int gauge_draw(');b=source.index('\nstd::pair<UiAction,int> map_label',a)
(work/'gauge_draw_under_test.hpp').write_text(source[a:b],encoding='utf-8')
assert '++panel_trace_generation;' in source[source.index('void diagnostics() {'):source.index('void diagnostics() {')+160]
compiler=r'D:\Qt\Qt6\Tools\mingw1310_64\bin\g++.exe'
env=os.environ.copy();env['PATH']=str(Path(compiler).parent)+os.pathsep+env['PATH']
for debug in [0,1]:
    exe=work/f'gauge_draw_tests_{debug}.exe'
    subprocess.run([compiler,'-std=c++17','-O2','-Wall','-Wextra',f'-DOANS_DEBUG={debug}','-static','-I'+str(work),'tests/gauge_draw_tests.cpp','-o',str(exe)],cwd=root,env=env,check=True)
    p=subprocess.run([str(exe)],cwd=root,env=env,capture_output=True)
    (out/f'callback-{debug}.log').write_bytes(p.stdout+p.stderr)
    print((p.stdout+p.stderr).decode('utf-8',errors='replace'),flush=True);p.check_returncode()
exe=work/'gl_state_tests.exe'
subprocess.run([compiler,'-std=c++17','-O2','-Wall','-Wextra','-DNOMINMAX','-DOANS_DEBUG=0','-static','-Isrc','tests/gl_state_tests.cpp','-lopengl32','-lglu32','-lgdi32','-o',str(exe)],cwd=root,env=env,check=True)
p=subprocess.run([str(exe)],cwd=root,capture_output=True,env=env)
(out/'gl-state.log').write_bytes(p.stdout+p.stderr)
print((p.stdout+p.stderr).decode('utf-8',errors='replace'),flush=True);p.check_returncode()

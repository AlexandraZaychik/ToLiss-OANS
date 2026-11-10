import csv,json,statistics,hashlib,platform
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'reports/runtime/detail-quality-ZUCK-21R-02L'
rows=list(csv.DictReader((out/'raw.csv').open()))
qualities=['ultra','max','xhigh','high','medium','low']
ranges=[2,1,.2,.1]
assert len(rows)==120, f'Expected 120 completed measurements, got {len(rows)}'
summary=[]
for r in ranges:
    group={q:[x for x in rows if x['quality']==q and float(x['range_nm'])==r] for q in qualities}
    for q,g in group.items():
        assert len(g)==5 and len({x['round'] for x in g})==5
        assert all(int(x['frames'])==33960 and int(x['base_builds'])+int(x['base_hits'])==33960 for x in g)
    baseline=statistics.median(float(x['mean_frame_ms']) for x in group['medium'])
    for q,g in group.items():
        med=lambda field:statistics.median(float(x[field]) for x in g)
        t=med('mean_frame_ms')
        summary.append(dict(range_nm=r,quality=q,relative_performance=100*baseline/t,mean_frame_ms=t,
            min_ms=min(float(x['mean_frame_ms']) for x in g),max_ms=max(float(x['mean_frame_ms']) for x in g),
            mean_base_build_ms=med('mean_base_build_ms'),base_builds=med('base_builds'),base_hits=med('base_hits'),
            mean_base_vertices=med('mean_base_vertices'),peak_base_vertices=med('peak_base_vertices'),
            p95_frame_ms=med('p95_frame_ms'),mean_dynamic_vertices=med('mean_dynamic_vertices')))
files=['tools/runway_detail_quality_benchmark.cpp','src/a350_map.cpp','src/lua_vertex_buffer.hpp','src/a350_adapter.lua','assets/a350/map_engine.lua','build/runway_detail_quality_benchmark.exe','build/lua/liblua.a','reports/runtime/detail-quality-ZUCK-21R-02L/buildings.json/dsf_buildings_ZUCK.json']
hashes={f:hashlib.sha256((root/f).read_bytes()).hexdigest() for f in files}
(out/'summary.json').write_text(json.dumps(dict(measurements=summary,sha256=hashes),indent=2),encoding='utf-8')
lines=['# ZUCK 21R → 02L 细节质量离线性能测试','','2026-11-10；Intel Core i7-13650HX；固定逻辑处理器 0；GCC 13.1、`-O2 -DOANS_DEBUG=0`，Lua 使用项目现有 O2 静态库。直接编译当前工作区生产源码，不使用旧测试程序。', '',
'场景：按用户选择在两个跑道入口之间直线移动（不是滑行道路径）。21R 入口 29.7471305, 106.6728143 → 02L 入口 29.7051727, 106.6367252；航向 216.758963°；距离 5823.422667 m；恒速 20 kt。完整轨迹 565.991404 秒，60 Hz，共 33960 帧（最后一帧钳制到终点）。', '',
'单侧 ARC，覆盖 2 / 1 / 0.2 / 0.1 NM；无交通、无路线覆盖层。每个组合预热 120 帧并强制复位，然后测量整条路径；共五轮，轮换并反转质量顺序。加载、地景扫描、预热和数据写出不计入计时。保留生产缓存：1 像素运动阈值、10 Hz 刷新上限；每帧调用基础地图与动态图层。离线按最快速度执行，不等待模拟时间。', '',
'地景采用本机启用优先级选出的 Global Airports apt.dat；建筑重新从当前 DSF 离线提取：103 个轮廓，35 个标记为航站楼，来自 2 个瓦片。地图合并后 94 个道面、312 个机位标签、10 个航站楼对象、51 个其他建筑对象。未运行模拟器或访问网络。', '',
'## 相对性能（Medium / middle = 100%，越高越快）','',
'公式：同一 NM 下，Medium 的五轮平均帧耗时中位数 ÷ 当前档位的五轮平均帧耗时中位数 × 100%。这是地图 CPU 处理速度，不是模拟器 FPS；不包含 OpenGL 提交、GPU 栅格化和驾驶舱合成。','']
def table(field,fmt):
    lines.extend(['| 质量 | 2 NM | 1 NM | 0.2 NM | 0.1 NM |','|---|---:|---:|---:|---:|'])
    for q in qualities:
        cells=[next(x for x in summary if x['quality']==q and x['range_nm']==r)[field] for r in ranges]
        lines.append('| '+q.capitalize()+' | '+' | '.join(format(x,fmt) for x in cells)+' |')
table('relative_performance','.1f')
lines+=['','## 平均每帧 CPU 耗时（ms，越低越好）',''];table('mean_frame_ms','.4f')
lines+=['','## 单次基础地图重建（ms，越低越好）',''];table('mean_base_build_ms','.3f')
lines+=['','## 平均基础地图顶点数',''];table('mean_base_vertices',',.0f')
lines+=['','## 首轮几何输出一致性','']
for r in ranges:
    groups=[];remaining=list(qualities)
    while remaining:
        q=remaining.pop(0);ref=(out/f'raw.csv.{q}.{r:.6f}.vertices.csv').read_bytes()
        equal=[other for other in remaining if (out/f'raw.csv.{other}.{r:.6f}.vertices.csv').read_bytes()==ref]
        remaining=[other for other in remaining if other not in equal]
        groups.append('/'.join([q]+equal))
    lines.append(f'- {r:g} NM：逐帧基础顶点计数相同的组为 {"；".join(groups)}。计数相同不保证几何坐标完全相同。')
lines+=['','## 五轮波动与缓存','', '| NM | 质量 | 中位数 ms/帧 | 最小–最大 ms/帧 | 基础重建次数 |','|---|---|---:|---:|---:|']
for x in summary:lines.append(f"| {x['range_nm']:g} | {x['quality']} | {x['mean_frame_ms']:.4f} | {x['min_ms']:.4f}–{x['max_ms']:.4f} | {x['base_builds']:.0f} |")
lines+=['','较小差异可能来自温度、调度和 Lua GC，不足以构成稳定排名。顶点数相同时不应把耗时差归因为画质差；顶点数也不能直接推导 GPU 耗时。中位数是五个整段平均值的中位数，各距离档独立归一化。', '',
'## 复现与原始数据','', '工具：`tools/runway_detail_quality_benchmark.cpp`。保持本次源码、资源、地景和编译参数，运行：','','```powershell',
'.\\build\\runway_detail_quality_benchmark.exe assets "G:\\SteamLibrary\\steamapps\\common\\X-Plane 12" reports/runtime/detail-quality-ZUCK-21R-02L/buildings.json/dsf_buildings_ZUCK.json reports/runtime/detail-quality-ZUCK-21R-02L/raw.csv 5 ZUCK 21R 02L 20', '```', '',
'原始 120 行结果、`summary.json`、`run.log`、`extraction.log` 和首轮逐帧顶点计数均位于 `reports/runtime/detail-quality-ZUCK-21R-02L/`。', '', '## 输入与程序 SHA-256','']
for f,h in hashes.items():lines.append(f'- `{f}`：`{h}`')
(root/'reports/ZUCK_21R_02L_DETAIL_QUALITY_BENCHMARK.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
for x in summary: print(f"{x['range_nm']:g} {x['quality']:6} {x['relative_performance']:6.1f}% {x['mean_frame_ms']:.4f}ms")

# -*- coding: utf-8 -*-
from pathlib import Path
import csv,json,statistics,hashlib
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'reports/runtime/render-audit-34'
def main():
    data={};metas={};checks=0
    for airport in ['ZUCK','ZGGG','ZBAA']:
        for variant in ['baseline','lean']:
            lines=(OUT/f'{airport}-{variant}.csv').read_text(encoding='utf-8').splitlines()
            assert lines[-1].startswith('PASS'),(airport,variant,'incomplete')
            metas[airport]=lines[0].split(',')
            assert int(metas[airport][-2])+int(metas[airport][-1])>0
            data[airport,variant]=list(csv.DictReader(lines[1:-1]))
        key=lambda r:(r['quality'],float(r['range']),r['traffic'],r['layer'])
        before={key(r):r for r in data[airport,'baseline'] if r['kind']=='cpu_dynamic'}
        after={key(r):r for r in data[airport,'lean'] if r['kind']=='cpu_dynamic'}
        assert before.keys()==after.keys()
        for k,r in before.items():
            assert (r['hash'],r['vertices'])==(after[k]['hash'],after[k]['vertices']),(airport,k,'vertex mismatch')
            checks+=1
    pixels=0
    for airport in ['ZUCK','ZGGG','ZBAA']:
        for range in ['0.200000','0.500000','2.000000']:
            for traffic in [0,32]:
                a=Image.open(OUT/f'{airport}-baseline-{range}-{traffic}.bmp').convert('RGB')
                b=Image.open(OUT/f'{airport}-lean-{range}-{traffic}.bmp').convert('RGB')
                assert a.tobytes()==b.tobytes(),(airport,range,traffic,'preview mismatch')
                pixels+=1
    def med(rows,kind,layer=None,traffic=None,field='mean_ms',quality=None):
        vals=[float(r[field]) for r in rows if r['kind']==kind and (layer is None or r['layer']==layer) and (traffic is None or int(r['traffic'])==traffic) and (quality is None or int(r['quality'])==quality)]
        return statistics.median(vals)
    summary={}
    for airport in ['ZUCK','ZGGG','ZBAA']:
        before=data[airport,'baseline'];after=data[airport,'lean']
        summary[airport]={'geometry':metas[airport][-4:]}
        for layer in ['surface','labels','dynamic']:
            a=med(before,'gl_immediate_cpu',layer);b=med(before,'gl_batch_cpu',layer)
            summary[airport]['gl_'+layer]={'immediate_ms':a,'batch_ms':b,'saved_ms':a-b,'speedup':a/b}
        for traffic in [0,32]:
            a=med(before,'cpu_dynamic','combined',traffic);b=med(after,'cpu_dynamic','combined',traffic)
            summary[airport]['dynamic_'+str(traffic)]={'before_ms':a,'lean_ms':b,'saved_ms':a-b,'percent':(a-b)/a*100}
        summary[airport]['max_surface_vertices']=max(int(r['vertices']) for r in before if r['kind']=='gl_immediate_cpu' and r['layer']=='surface')
        summary[airport]['frame_normal']={v:med(data[airport,v],'cpu_dual_frame','normal') for v in ['baseline','lean']}
        summary[airport]['frame_split']={v:med(data[airport,v],'cpu_dual_frame','route_split') for v in ['baseline','lean']}
    (OUT/'summary.json').write_text(json.dumps({'airports':summary,'dynamic_vertex_checks':checks,'preview_pixel_checks':pixels,'immediate_batch_pixel_checks':108},indent=2),encoding='utf-8')
    # Preview sheet: full map buffer with no cockpit or host UI, same taxiway poses.
    sheet=Image.new('RGB',(1800,1280),'#141414');draw=ImageDraw.Draw(sheet)
    for col,airport in enumerate(['ZUCK','ZGGG','ZBAA']):
        for row,traffic in enumerate([0,32]):
            im=Image.open(OUT/f'{airport}-baseline-0.500000-{traffic}.bmp').convert('RGB')
            sheet.paste(im,(col*600,row*640+40))
            draw.text((col*600+12,row*640+12),f'{airport} | 0.5 NM ARC | {traffic} traffic',fill='white')
    sheet.save(OUT/'preview-sheet.png')
    text=['# 第 3、4 项离线评估（2026-11-10）','',
    '结论：第 3 项值得做成受控的小改动，仅对高顶点量三角形缓冲区启用批量提交；第 4 项暂不建议大改。当前只新增离线测试工具、生成测试副本和报告，没有修改第 3、4 项的生产实现。','',
    '## 测试范围与限制','',
    '- GCC 13.1、`-O2 -DOANS_DEBUG=0`，当前源码与现有 Lua 静态库；只编译独立离线工具，未编译或安装插件，未启动模拟器、未联网。',
    '- Intel RaptorLake-S Mobile Graphics Controller，OpenGL 4.6；固定逻辑 CPU 0。此离线上下文走核显，不能外推为模拟器独显路径的收益或 FPS。',
    '- 本机启用优先级选出的 Global Airports apt.dat；三个机场均加载 DSF 建筑缓存。每场景取机场参考点附近一段实际滑行道的中点和方向。',
    '- ARC，0.2 / 0.5 / 2 NM；Medium / Ultra；0 交通及 32 交通压力场景（后者同时含 32 个十字和 32 个旗标）。各参数组合先预热。',
    '- 动态层计时每组合 300 次；双侧移动计时 600 帧，15 kt 近似位移、3°/s 转弯，保留生产缓存和 10 Hz 限制；另测有路线时的罗盘/飞机分层调用，不包含路线裁剪本身。',
    '- GL 每次提交前清空队列，绘制函数本身单独计时（50 次）；另记录包含 `glFinish` 的同步总耗时（40 次）。本文主要比较 CPU 提交时间，不把该值与 GPU 时间相加。图层单独绘制，未模拟完整驾驶舱合成。',
    '- 表格使用各质量、距离、交通组合平均耗时的中位数，非模拟器帧率；短时测量存在后台调度噪声，约几微秒的差异不应作为稳定收益。','',
    '## 第 3 项：逐顶点提交与批量提交','',
    '原型使用兼容模式的 `glDrawArrays` 和已有交错顶点数据，保存/恢复客户端数组与数组缓冲绑定；不引入 VBO、持久映射、着色器或新的 GL 版本要求。测量亮度为 1。','',
    '| 机场 | 最大表面顶点数 | 表面逐顶点 CPU ms | 表面批量 CPU ms | 单次节省 ms | 加速倍数 |','|---|---:|---:|---:|---:|---:|']
    for airport,s in summary.items():
        g=s['gl_surface'];text.append(f"| {airport} | {s['max_surface_vertices']:,} | {g['immediate_ms']:.4f} | {g['batch_ms']:.4f} | {g['saved_ms']:.4f} | {g['speedup']:.2f}× |")
    text+=['','这些是表面缓冲区重建并提交时的收益。缓存纹理命中的帧只绘制贴图，不会重复提交整张底图，不能把上表的节省直接当成每帧节省。','', '| 机场 | 同步总耗时原版 ms | 批量 ms |', '|---|---:|---:|']
    for airport in summary:
        rows=data[airport,'baseline'];a=med(rows,'gl_immediate_finish','surface');b=med(rows,'gl_batch_finish','surface')
        text.append(f'| {airport} | {a:.4f} | {b:.4f} |')
    text+=['','同步总耗时包含绘制准备、清屏、提交和 GPU 等待，下降远小于 CPU 提交加速倍数；不能把 CPU 提交快四倍解读成地图或模拟器快四倍。','',
    '| 机场 | 标注层节省 ms | 动态层节省 ms |','|---|---:|---:|']
    for airport,s in summary.items():text.append(f"| {airport} | {s['gl_labels']['saved_ms']:.4f} | {s['gl_dynamic']['saved_ms']:.4f} |")
    text+=['','建议范围：','',
    '1. 优先在统一三角形提交函数内加批量分支和原路径回退；先限定高顶点量缓冲区，例如以 8K–16K 顶点作候选阈值，再通过实机数据定阈值。此阈值是工程建议，本轮没有证明精确的最优分界。',
    '2. 小标注、少量飞机符号保留现状，避免数组状态保存开销抵消收益；暂不推进完整 VBO/着色器重构。',
    '3. 正式实现前补测非 1 亮度的逐字节颜色量化、纹理单元/客户端纹理单元、已有 VAO/VBO/数组启用状态、SDK 状态同步及 macOS 兼容路径。本轮像素等价仅覆盖干净的离线兼容 GL 上下文。','',
    '## 第 4 项：动态层参数精简原型','',
    '仅在隔离源码副本中：动态层不传静态 crosses/flags/FMS/地图选中跑道；罗盘层不传交通表；不复制完整 A350View 到 side-3 动态缓存。27 参数入口、Lua 绘制算法和动态顶点生成均保留，不做帧缓存。压力场景仍需传真实交通给飞机层，不能把交通全部省掉。','',
    '| 机场 | 无交通原版 ms | 精简后 ms | 节省 ms | 32 交通原版 ms | 精简后 ms | 节省 ms |','|---|---:|---:|---:|---:|---:|---:|']
    for airport,s in summary.items():
        a,b=s['dynamic_0'],s['dynamic_32'];text.append(f"| {airport} | {a['before_ms']:.4f} | {a['lean_ms']:.4f} | {a['saved_ms']:.4f} | {b['before_ms']:.4f} | {b['lean_ms']:.4f} | {b['saved_ms']:.4f} |")
    text+=['','上表为一次合并动态层调用，未包含 GL。大机场静态几何多，并不意味着动态参数传递同样重；罗盘、飞机和交通文本本身的生成仍需执行。完整双侧 CPU 帧数据保存在原始 CSV 与 summary.json，包含底图更新，因缓存重建分布和噪声不能用它准确归因这几微秒的参数开销。','',
    '建议：暂时保持生产接口；若后续顺手优化，只裁掉各层不使用的静态表和完整视图复制，不重写 Lua API。按视图跨帧复用不能只检查地图视图：位置、航向、磁航向、交通文本/姿态及显示条件都可能变化。按帧复用应先在模拟器中记录同一侧、同一层、同一周期的重复调用比例；源码中 gauge 支持 rt=1/2 仅表明存在多个绘制阶段，不能证明实际有多少可复用调用。本轮未测到实机重复率，因此不建议直接增加帧缓存。','',
    '## 等价性与产物','',
    f'- {checks} 个动态层场景，原版与精简副本的顶点数量和字段哈希全部一致。',
    f'- {pixels} 张原版/精简离线预览逐像素一致；108 组真实机场图层的逐顶点/批量提交逐像素一致。',
    '- 这些检查不等同于对真实驾驶舱 GL 状态、非 1 亮度或所有模式的覆盖。',
    '- 原始 CSV、运行错误日志、输入源码 SHA-256、预览与 summary.json 位于 `reports/runtime/render-audit-34/`。',
    '- 复现：`python tools/run_offline_render_audit.py`。工具中记录了本机编译器、模拟器数据与建筑缓存路径；不会构建插件。','',
    '![三个机场离线预览](runtime/render-audit-34/preview-sheet.png)','']
    (ROOT/'reports/RENDER_OPTIMIZATION_3_4_AUDIT.md').write_text('\n'.join(text),encoding='utf-8')
    print(json.dumps(summary,indent=2));print('PASS equality:',checks,pixels)
if __name__=='__main__':main()

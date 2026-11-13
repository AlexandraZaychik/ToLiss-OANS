# 机型适配模块

现有 ToLiss 机模实现归为 `toliss`；ACF 的 `acf/_studio` 等于 `Laminar Research` 时归为 `lr`；Zibo B738 使用独立的 `zibo` 后端。地图、机场数据、跑道告警和 UI 使用统一结果；厂商 DataRef、显示布局提取和 FMS 来源由适配模块负责。各机型的自动采集、按键触发、重试及告警判定规则见[检测逻辑](检测逻辑.md)。

```text
src/
  aircraft_profile.hpp       # 通用显示数据结构、JSON 校验与加载
  aircraft_geometry.hpp      # 机体尺寸及现有参考点标定
  aircraft_limits.hpp        # ICAO 对应的机场跑道尺寸限制
  fms.hpp / fms.cpp          # FmsPlan、起降机场/跑道/备降、航路外机场判断
  fms_source.hpp             # FmsSource 接口与 FmsPort 生命周期
  aircraft/
    backend.hpp             # 适配契约、统一数据读取入口
    backend.cpp             # 已实现适配器的注册和探测
    toliss/
      backend.hpp / .cpp     # AirbusFBW 映射、模式/供电/IRS/亮度/UV 选择
      profile.hpp / .cpp     # ToLiss ACF/OBJ、六屏布局、分辨率和亮度索引提取
      mcdu.hpp / .cpp        # 彩色文本解析、页面识别、采集/恢复状态机
      mcdu_xplm.hpp / .cpp   # MCDU DataRef、按键监听、ECAM 阶段和日志
    lr/
      identity.hpp          # ACF studio 精确匹配，不依赖目录或 ICAO
      backend.hpp / .cpp    # LR 身份、config/lr 加载和 A333 ND DataRef
      display.hpp           # ND 模式/量程、显示状态、惯导有效性、亮度及显示交换
      mcdu.hpp / .cpp        # LR 独立页面解析与采样状态机
      mcdu_screen.hpp        # 原生 CDU 文本行转换
      mcdu_xplm.hpp / .cpp   # sim/FMS 按键、双按 F-PLN、FWS 阶段与恢复
```

## 数据与读取策略

- `AircraftProfile` 保存解析后的 ND/PFD UV、亮度索引和 ND 点击角点。JSON 校验与机模厂商无关；六屏批次、OBJ 动画和 `AirbusFBW` 操纵器等提取规则属于 `toliss/profile.cpp`。
- 主插件只通过 `Backend` 读取标准化 ND 模式、UV、亮度、供电和位置有效性。ToLiss 的正常/回退分辨率、显示交换、模式编码和 IRS 门控在 `toliss/backend.cpp` 中解释。
- `FmsPlan` 不依赖 MCDU。`FmsSource` 可以通过厂商 API、DataRef 或另一套 CDU 逻辑生成相同结果；UI 不接触 ToLiss 的 `FmsSampler` 和页面枚举。
- `FmsSource::uses_mcdu_sampling_setting()` 声明来源的后台采样是否受 CDU 采样开关控制，默认受控。具体机型的采样时机、触发键和开关行为见[检测逻辑](检测逻辑.md)。
- ToLiss 与 LR 使用独立 MCDU 解析和采集状态机；Zibo 使用独立 DataRef 来源。页面识别、机组操作避让、采样和页面恢复规则见[检测逻辑](检测逻辑.md)。
- 机体尺寸和机场跑道尺寸限制保留为独立数据。RWY AHEAD 按适配器厂商 ID 和 ICAO 联合查找尺寸，再读取当前 ACF 的 `acf/_cgZ` 与 `_gear/0/_gear_z` 标定参考点；包络切换会清除告警状态。尺寸或标定缺失、无效时，整套回退到原先的 ToLiss A320/A20N 包络，禁止跨厂商借用同名机型数据。
- `lr/A333` 使用用户提供的国航 A330/A340 FCOM 尺寸：机长 63.69 m、翼展 60.30 m、机鼻至前轮 6.67 m。机场跑道筛选沿用 A339 的 1600 m / 45 m。`toliss/A339` 的翼展及机鼻至前轮尺寸尚缺，告警包络仍回退到 A320；LR 告警显示的可用性由适配器单独控制。

## 选择与生命周期

`XPluginStart` 只绑定模拟器接口。首次飞行循环按 `zibo`、`lr`、`toliss` 的顺序探测适配器。

首次探测前为 `WAITING FOR AIRCRAFT IDENTIFICATION`，不提前执行配置检查或写“不支持”错误。首次探测、适配器变化及配置/接口就绪状态变化后自动刷新接口报告。普通日志记录 `manufacturer`、`family`、`ICAO` 和机模文件名。

LR 通过 `XPLMGetNthAircraftModel(0, ...)` 取得当前 ACF，读取元数据 `acf/_studio`；去除首尾空白后，值精确等于 `Laminar Research` 就识别为 `lr`，不附加 A333 ICAO 或目录名称条件。按当前 ACF 路径和修改时间缓存结果，切换飞机或文件变动后重新读取。该规则优先于 ToLiss 名称识别。LR 配置从 `config/lr/<小写ICAO>.json` 加载；当前提供实际 LR A333 提取结果。

LR A333 的 19 个显示绑定全部来自 LR / 标准接口。左右屏幕状态独立读取；具体模式、量程、显示状态、惯导有效性及交换屏幕时的叠加条件见[检测逻辑](检测逻辑.md)。

LR 不绑定 ToLiss 接口，MCDU/FMS 使用独立的 LR 来源。PFD/ND 跑道告警复用公共绘制流程和 LR 各屏 UV、亮度索引；具体显示门控及实机验证边界见[检测逻辑](检测逻辑.md)。

满足以下任一条件就识别为 ToLiss，字符串匹配均忽略大小写：

- 任意已注册 DataRef 的名称包含 `toliss`，例如 `toliss_airbus/pfdoutputs/general/fd_configuration` 或 `toliss_airbus/init/costindex`。枚举全部已注册名称，不限定固定列表，也不要求其数值为非零。
- `sim/aircraft/view/acf_ui_name` 的值包含 `toliss`。

识别与接口就绪分别判断。识别成功即可从 `config/toliss/` 加载显示配置；ND 接口未就绪时继续等待，不提前启动 MCDU 监听。仅有 Airbus ICAO 或 `AirbusFBW` ND 接口不构成厂商识别依据。

DataRef 名称枚举使用通过 `XPLMFindSymbol` 动态获取的 SDK 4 注册表接口。旧宿主若没有枚举接口，仍可通过 `acf_ui_name` 识别。

选中适配器后绑定该适配器的 DataRef、加载显示数据并创建 FMS 来源。接口提供方可以延迟出现，后续循环会重试。未识别的机模保持 `AIRCRAFT_BACKEND id=NONE`，不加载 ToLiss 显示数据，也不运行其 MCDU 采集。

切换适配器时，先注销旧来源的按键监听，清除旧 DataRef 映射、显示数据和航路结果，再创建新的来源。飞机重载会清理采集状态并重置页面跟踪；机场位置重载保留原有主页面跟踪行为。

诊断文件输出 `AIRCRAFT_BACKEND id=lr/toliss/NONE manufacturer="Laminar Research/ToLiss/UNKNOWN"`，并通过 `AIRCRAFT_IDENTIFICATION state=WAITING/IDENTIFIED/UNSUPPORTED` 标识识别阶段。LR 另外输出 `LR_ADAPTER display_ready=0/1 fms_source=LR_MCDU2` 和两侧 `LR_ND` 的模式、量程、显示状态、惯导有效性与亮度。通用插件负责诊断调用，各适配器和 FMS 来源输出自己的诊断细节。

## 新增 LR330、AS346 等适配

1. 在 `src/aircraft/<family>/` 创建独立模块，实现 `Backend`。探测规则应识别实际接口提供方，同一 ICAO 的其他机模不能直接复用 ToLiss 解析规则。
2. 独立实现显示数据加载/提取与 UV 选择，向公共渲染层提供标准化结果。公共加载入口按适配器 ID 选择 `config/<family>/`。ToLiss 使用 `config/toliss/<ICAO或ID>.json`，不回退到 `config/` 根目录；ID 配置优先、ICAO 配置次之、缺失时按 ID 生成的规则保持不变。不同厂商的相同 ICAO 配置由目录隔离。
3. 实现自己的 `FmsSource`。如果机模有直接航路 API，可以直接发布 `FmsPlan`，无需实现 MCDU 页面、按键或恢复流程。
4. 在 `aircraft/backend.cpp` 注册适配器，并更新 CMake 和 PowerShell 构建源列表。通用 `oans_core` 不链接 ToLiss MCDU 实现；它单独构建为 `oans_toliss_fms`。
5. 验证显示布局、点击投影、机模重载、位置有效性及航路数据，再在真实机模中确认 ND 最终显示。原生模拟宿主回归只证明隔离环境中的接口和生命周期行为。

LR330 当前已完成厂商识别、配置提取和 ND 地图接入，模拟器内显示待用户测试；FMS 已接入独立 LR MCDU2 来源，实机探针已读到起降机场、跑道和备降；完整插件内显示仍需验证。AS346 尚未实现。

## LR A333 配置提取

`tools/extract_lr_profile.py --model <当前A330.acf>` 从 ACF 指定的 `cockpit/A330_cockpit.obj` 提取静态面板四边形，通过 ACF 的 `Blue Glow ND/PFD Capt/FO` 面板坐标定位左右显示屏，并交叉核对 OBJ 亮度旋钮和 ACF `LIGHT_RHEOSTAT` 索引。

输出为 `config/lr/a333.json`，来源、哈希、屏幕顶点和坐标变换另存于 `reports/LR_A333_PROFILE_20261030.json`。当前机模面板为 2048×1024，ND 亮度索引 `[1,4]`，PFD `[0,5]`，均对应 `sim/cockpit2/switches/instrument_brightness_ratio`。单一纹理布局在公共 JSON 的两组分辨率槽中重复保存。ND 角点按左上、右上、右下、左下排列，包含 ACF 附件偏移及名义重心偏移。提取过程不修改 ACF/OBJ。

## 验证入口

```powershell
pwsh -NoProfile -File scripts/build.ps1
pwsh -NoProfile -File scripts/test-native.ps1 -BuildFlavor Release
pwsh -NoProfile -File scripts/test-native.ps1 -BuildFlavor Debug
```

`fms_source_tests` 使用完全不依赖 XPLM/MCDU 的模拟 API 来源，检查公共 FMS 接口、来源替换和旧航路清理。原生宿主另外检查任意 DataRef 名称命中、仅 UI 名称命中、大小写匹配、未命中时注销旧监听，以及 `config/toliss/` 的加载和生成；故意无效的根目录配置不会影响家族子目录。

## ECAM / FWS 飞行阶段

阶段定义根据用户提供的 ECAM 飞行阶段示意图转录；图中未定义的高度基准不自行补充。阶段用于 ToLiss / LR 自动采集调度，不等同于单独的空地状态。

| 阶段 | 起始条件 | 结束条件 | 说明 |
| --- | --- | --- | --- |
| 1 | ELEC PWR | 1ST ENG STARTED | 飞机通电至第一台发动机启动 |
| 2 | 1ST ENG STARTED | 1ST ENG TO PWR | 第一台发动机启动至达到起飞推力 |
| 3 | 1ST ENG TO PWR | 80 kt | 起飞推力至滑跑达到 80 节 |
| 4 | 80 kt | LIFTOFF | 滑跑 80 节至离地 |
| 5 | LIFTOFF | 1500 ft | 离地至爬升达到 1500 英尺 |
| 6 | 1500 ft | 800 ft | 越过 1500 英尺后的飞行，至进近下降达到 800 英尺 |
| 7 | 800 ft | TOUCHDOWN | 进近下降达到 800 英尺至接地 |
| 8 | TOUCHDOWN | 80 kt | 接地至着陆滑跑减速到 80 节 |
| 9 | 80 kt | 2ND ENG SHUTDN | 着陆后减速到 80 节至第二台发动机关车 |
| 10 | 2ND ENG SHUTDN | 5MN AFTER | 第二台发动机关车后的 5 分钟 |

ToLiss 读取 `AirbusFBW/ECAMFlightPhase`；LR A330 读取只读 double `laminar/A333/data/flight_phase`，其值来自 LR XLua 的 ECAM/FWS 阶段。初始化和采样事务期间也会按需刷新。LR 的阶段源不是 MCDU/FMGS 的 PRE-FLIGHT、CLB 等阶段枚举。阶段刷新周期和采集阶段限制见[检测逻辑：ToLiss / LR330](检测逻辑.md#toliss--lr330)。

## ToLiss / LR330 FMS 采集

两种机型各自维护 `mcdu.*` 和 `mcdu_xplm.*`，不共用页面解析、按键映射或采集状态机；主插件只消费统一的 `FmsPlan`。FMS 信息包括起飞机场 / 跑道、目的地机场 / 跑道及备降机场。LR 读取 `sim/cockpit2/radios/indicators/fms_cdu2_text_line0..15` 和 `sim/FMS2/*`；ToLiss 读取 `AirbusFBW/MCDU...` 文本与按键接口。

自动采集、按键触发、静默等待、重试及起飞信息保护规则统一见[检测逻辑：ToLiss / LR330](检测逻辑.md#toliss--lr330)。设置项 `auto info from mcdu2` 对应 INI 键 `fms_mcdu2_sampling`。

适配器内部的页面处理细节：

- 页面条件根据被操作侧最近一次主页面键记录，包含对应主页面的子页面；单独按 INIT、F-PLN 等主页面键不会触发航路采集。SEC INDEX 与 SEC F-PLN 使用同一主页面归类。
- INIT 直读在下一次插件回调执行；输入栏非空不影响直读。按键直读 / 翻页采集先更新当前可读取到的端点，保留另一侧已有信息。
- LR 进入、复核或恢复 F-PLN 时按两次 F-PLN 键，间隔 250 ms；在稳定页面确认后读取 F-PLN，核对 FUEL PRED 目的地，再回到 F-PLN 复核端点并恢复原页面。机组按键会取消延迟中的第二次 F-PLN 键。
- ToLiss 将 `DATA INDEX` 识别为 DATA 主页面，并实现 A/C STATUS / INIT B 页面恢复。

LR 开发探针曾读取到 ZUUU/02L、ZBAA/01 和备降 ZBAD，并恢复 DATA INDEX。探针只操作导航页，不安装 / 重载插件；记录在 `reports/runtime/lr-a333-20261102/fms-roundtrip.json`。该探针结果不代表当前插件二进制已在模拟器内完成同样验证。

### 飞行阶段驱动的 OANS 自动开关

- ToLiss 和 LR A330 分别在 ECAM / FWS 阶段 **7→8** 时检查左右两侧；仅打开尚未打开的 OANS，不因之后在阶段 8 手动关闭而重复开启。若初始加载时已处于阶段 8，不补触发。
- OANS 内部以 ARC 形式显示并跟随飞机，不改飞机 ND 模式旋钮；物理 ND 量程置为 10 NM 以满足叠加条件。按飞机接地时的位置和航向匹配实际跑道：跑道全长 ≤2500 m 使用 0.5 NM，>2500 m 使用 1 NM，未匹配到跑道时回退 1 NM（随五档量程统一减半）。该选择独立于 FMS 到达跑道和地图选择。
- `Setting > Close oans when TO` 控制阶段 **4→5** 时是否关闭两侧 OANS；关闭后仍可手动重新打开。此逻辑独立于 MCDU2 采集，当前默认关闭（`close_when_takeoff=0`）。

## Zibo B737-800X

### 机型识别与显示配置

使用独立 `zibo` 后端，配置文件为 `config/zibo/b738.json`。从当前 ACF 元数据读取 `acf/_studio` 和 ICAO；要求厂商字段为 `Laminar Research modified by Zibo and Twkster` 且 ICAO 为 `B738`，因此默认 Laminar Research 737 不会被识别为 Zibo。普通和 4K ACF 的提取结果相同；当前检查版本为 4.04.36。

使用 `tools/extract_zibo_profile.py --model <b738.acf>` 重提屏幕布局。提取时根据 OBJ 正常显示的 hide 条件选择屏幕，拒绝带旋转 / 平移动画的批次，并应用 ACF 附件和重心偏移。2048×2048 面板只有一套布局，在公共 JSON 的两组分辨率槽中重复保存；来源哈希及提取结果记录于 `reports/ZIBO_B738_PROFILE_20261102.json`。

物理 ND 量程必须为 **5 NM**：左右 `laminar/B738/EFIS/.../map_range` 值为 0；其他量程不显示 OANS 叠加。MAP=2、PLN=3；MAP 的 `exp_map=1` 对应公共 ARC，居中 MAP 对应 NAV，PLN 对应 PLAN。APP / VOR 模式不叠加；VSD 开启时不叠加。ND 显示必须是内侧 ND（`display_units/idu_cpt` / `idu_fo` 为 0），且 `irs/nd_allign` / `nd_allign2` 为 0（无 ND 惯导告警）。ND 亮度通道为 2 / 3，PFD 为 0 / 1，来源为 `sim/cockpit2/switches/instrument_brightness_ratio`。左右侧独立判断。

### FMS DataRef 来源

- 起飞 / 目的地机场和跑道分别读取 `laminar/B738/fms/ref_icao`、`ref_runway`、`dest_icao`、`dest_runway`。读取触发、延迟、取消条件及备降支持范围见[检测逻辑：Zibo 737](检测逻辑.md#zibo-737)。
- 来源只读飞机 DataRef，不发送按键或写入飞机 DataRef；航路清空后会清除旧端点。
- 飞行阶段来自 `laminar/B738/FMS/flight_phase`，约每 3 秒刷新供诊断使用；ND / PFD 显示状态由显示处理读取，与 FMS 航路字段的读取分开。

### 跑道接近告警

- 机场跑道筛选使用同一条跑道长度至少 **1200 m**、宽度至少 **30 m**。
- Zibo 机体纵向包络 39.50 m、翼展 34.40 m（半翼展 17.20 m），来源为用户提供的[尺寸图](../reports/references/runway-proximity/b738-user-geometry.png)。图中 38.00 m 不含末端突出部分；机鼻至前轮约 4.40 m 是按图比例估算，并非图中标注值。算法使用水平矩形包络；图示高度、主轮轮距、轴距和水平尾翼展不参与当前算法。
- 机鼻 / 机尾相对模拟器位置的距离按当前 ACF 的 `acf/_cgZ` 与 `_gear/0/_gear_z` 换算，不借用 A320 参考点。缺少 ACF 标定时走现有默认包络回退。
- 双侧 PFD / ND 复用公共跑道接近告警绘制，使用各自 JSON UV 和亮度通道。告警不受 OANS 地图开关和物理 5 NM 量程限制；ND 在 MAP / PLN 且 VSD 关闭时显示。PFD 不依赖 ND 模式、量程、亮度或显示状态。正常 PFD / ND 通过 `display_units/odu_cpt`、`odu_fo`、`idu_cpt`、`idu_fo` 判断；显示关闭、切换或缺少对应接口时不绘制该屏告警。显示控制 DataRef 依据当前本地 cockpit OBJ 的 hide 条件，保守支持正常布局。
- 离线检查不能证明模拟器内的实际绘制顺序、位置和最终显示；这些仍需在模拟器中验证。

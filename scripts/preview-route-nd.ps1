param(
    [string]$SimulatorRoot='G:\SteamLibrary\steamapps\common\X-Plane 12',
    [string]$CompilerDirectory='D:\Qt\Qt6\Tools\mingw1310_64\bin',
    [string]$SdkRoot='E:\cxx_project\_lib\SDK'
)
$ErrorActionPreference='Stop'
$previewRoot=Split-Path -Parent $PSScriptRoot
$compiler=Join-Path $CompilerDirectory 'g++.exe'
$previewBuild=Join-Path $previewRoot 'build/nd-route-preview'
$previewReports=Join-Path $previewRoot 'reports/runtime/route-plan-20261103'
New-Item -ItemType Directory -Force $previewBuild,$previewReports | Out-Null
Push-Location $previewRoot
try {
    $flags=@('-std=c++17','-O2','-static','-Isrc','-DOANS_DEBUG=0','-DIBM=1','-DAPL=0','-DLIN=0','-DXPLM200','-DXPLM210','-DXPLM300','-DXPLM301','-DXPLM303','-DNOMINMAX','-DLIL=1',"-I$SdkRoot/CHeaders/XPLM",'-Ivendor/lua-5.4.9/src','-Ivendor/xptools/DSF','-Ivendor/xptools/Utils','-Ivendor/xptools/lzma19/C')
    & $compiler @flags '-shared' '-DXPLM=1' '-DOANS_PREVIEW_GL' 'tests/mock_xplm.cpp' '-lopengl32' "-Wl,--out-implib,$previewBuild/libXPLM.a" '-o' "$previewBuild/XPLM_64.dll"
    if($LASTEXITCODE -ne 0){throw 'Preview SDK compilation failed'}
    $sources=@('src/core.cpp','src/runway_ahead.cpp','src/fms.cpp','src/aircraft/toliss/mcdu.cpp','src/aircraft/backend.cpp','src/aircraft/magknight/backend.cpp','src/aircraft/toliss/backend.cpp','src/aircraft/zibo/backend.cpp','src/aircraft/zibo/fms_xplm.cpp','src/aircraft/lr/backend.cpp','src/aircraft/lr/mcdu.cpp','src/aircraft/lr/mcdu_xplm.cpp','src/aircraft/toliss/mcdu_xplm.cpp','src/nd_ui.cpp','tools/route_nd_preview.cpp','src/aircraft/toliss/profile.cpp','src/a350_map.cpp','src/dsf_buildings.cpp')
    $objects=@()
    foreach($source in $sources) {
        $object=Join-Path $previewBuild (($source -replace '[/.]','_')+'.o')
        $objects+=$object
        $dependencies=@($source)
        if($source -eq 'tools/route_nd_preview.cpp'){$dependencies+=@('src/plugin.cpp','src/route_plan.hpp')}
        $newest=($dependencies | ForEach-Object {(Get-Item -LiteralPath $_).LastWriteTime} | Sort-Object -Descending | Select-Object -First 1)
        if(!(Test-Path -LiteralPath $object) -or (Get-Item -LiteralPath $object).LastWriteTime -lt $newest) {
            & $compiler @flags '-c' $source '-o' $object
            if($LASTEXITCODE -ne 0){throw "Preview object compilation failed: $source"}
        }
    }
    & $compiler '-static' @objects 'build/lua/liblua.a' 'build/dsf/libdsf.a' "$previewBuild/libXPLM.a" '-lopengl32' '-lglu32' '-lgdi32' '-lwinhttp' '-o' "$previewBuild/route_nd_preview.exe"
    if($LASTEXITCODE -ne 0){throw 'Preview link failed'}
    foreach($route in @(@{icao='ZUCK';text='[ZUCK] 608-P2-E8-D-E11-RW20L'},@{icao='ZUUU';text='rw20r a8 a b7 b c6 t8 143 [ZUUU]'})) {
        & "$previewBuild/route_nd_preview.exe" $previewRoot $SimulatorRoot $route.text (Join-Path $previewReports ($route.icao+'-ND.bmp'))
        if($LASTEXITCODE -ne 0){throw "ND preview failed: $($route.icao)"}
    }
} finally {Pop-Location}

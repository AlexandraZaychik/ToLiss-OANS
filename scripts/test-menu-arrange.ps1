param(
    [string]$Python = 'python',
    [string]$SdkRoot = 'E:\cxx_project\_lib\SDK',
    [string]$CompilerDirectory = 'D:\Qt\Qt6\Tools\mingw1310_64\bin'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$testRoot = Join-Path $projectRoot 'build/menu-arrange'
$compiler = Join-Path $CompilerDirectory 'g++.exe'
Push-Location $projectRoot
try {
    & $Python -X utf8 'tests/menu_arrange_tests.py'
    if($LASTEXITCODE -ne 0){throw 'Menu editor tests failed'}
    foreach($library in @('build/lua/liblua.a','build/dsf/libdsf.a')) {
        if(!(Test-Path -LiteralPath $library)){throw 'Build the Lua and DSF libraries first with scripts/build-lua.ps1 and scripts/build-dsf.ps1'}
    }
    New-Item -ItemType Directory -Force $testRoot | Out-Null
    $sdkFlags = @('-DIBM=1','-DAPL=0','-DLIN=0','-DXPLM200','-DXPLM210','-DXPLM300','-DXPLM301','-DXPLM303',"-I$SdkRoot/CHeaders/XPLM")
    & $compiler '-std=c++17' '-O0' '-static' '-shared' '-DXPLM=1' @sdkFlags 'tests/mock_xplm.cpp' '-o' (Join-Path $testRoot 'XPLM_64.dll')
    if($LASTEXITCODE -ne 0){throw 'Menu mock SDK build failed'}
    & $compiler '-std=c++17' '-O0' '-static' '-Isrc' 'tests/menu_layout_native.cpp' '-o' (Join-Path $testRoot 'menu_layout_native.exe')
    if($LASTEXITCODE -ne 0){throw 'Menu native host build failed'}
    $variantRoot = Join-Path $testRoot 'reversed-source'
    New-Item -ItemType Directory -Force $variantRoot | Out-Null
    Copy-Item -LiteralPath 'src/plugin.cpp','src/menu_layout.hpp' -Destination $variantRoot -Force
    $reverseScript = @'
from pathlib import Path
import sys
sys.path.insert(0, 'scripts')
from menu_arrange import parse, read, reorder
source = read(Path('src/menu_layout.def'))
entries = parse(source)
def visit(parent):
    result = []
    for entry in reversed([e for e in entries if e.parent == parent]):
        result.append(entry)
        result.extend(visit(entry.id))
    return result
Path(sys.argv[1]).write_bytes(reorder(source, visit('root')).encode('utf-8'))
'@
    & $Python -X utf8 -c $reverseScript (Join-Path $variantRoot 'menu_layout.def')
    if($LASTEXITCODE -ne 0){throw 'Reversed menu fixture generation failed'}
    $simRoot = Join-Path $testRoot 'sim'
    New-Item -ItemType Directory -Force $simRoot | Out-Null
    $model = Join-Path $simRoot 'fixture.acf'
    'I' | Set-Content -LiteralPath $model -Encoding ascii
    $common = @('-std=c++17','-O0','-static','-shared','-DOANS_DEBUG=0','-DNOMINMAX','-DLIL=1','-Isrc','-Ivendor/lua-5.4.9/src','-Ivendor/xptools/DSF','-Ivendor/xptools/Utils','-Ivendor/xptools/lzma19/C')
    $sources = @('src/core.cpp','src/runway_ahead.cpp','src/fms.cpp','src/aircraft/toliss/mcdu.cpp','src/aircraft/backend.cpp','src/aircraft/toliss/backend.cpp','src/aircraft/zibo/backend.cpp','src/aircraft/zibo/fms_xplm.cpp','src/aircraft/lr/backend.cpp','src/aircraft/lr/mcdu.cpp','src/aircraft/lr/mcdu_xplm.cpp','src/aircraft/toliss/mcdu_xplm.cpp','src/nd_ui.cpp','src/aircraft/toliss/profile.cpp','src/a350_map.cpp','src/dsf_buildings.cpp')
    foreach($variant in @('current','reversed')) {
        $package = Join-Path $testRoot $variant
        $binary = Join-Path $package '64/win.xpl'
        New-Item -ItemType Directory -Force (Join-Path $package '64') | Out-Null
        Copy-Item -LiteralPath 'config.ini','config','assets' -Destination $package -Recurse -Force
        $pluginSource = if($variant -eq 'reversed'){Join-Path $variantRoot 'plugin.cpp'}else{'src/plugin.cpp'}
        & $compiler @common @sdkFlags @sources $pluginSource 'build/lua/liblua.a' 'build/dsf/libdsf.a' (Join-Path $SdkRoot 'Libraries/Win/XPLM_64.lib') '-lopengl32' '-lglu32' '-lgdi32' '-lwinhttp' '-o' $binary
        if($LASTEXITCODE -ne 0){throw "Menu plugin build failed: $variant"}
        [string[]]$extra = if($variant -eq 'reversed'){@('--reverse')}else{@()}
        & (Join-Path $testRoot 'menu_layout_native.exe') (Join-Path $testRoot 'XPLM_64.dll') $binary $simRoot $model @extra
        if($LASTEXITCODE -ne 0){throw "Native menu regression failed: $variant"}
    }
} finally { Pop-Location }

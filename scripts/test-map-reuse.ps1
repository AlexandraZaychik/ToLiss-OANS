param(
    [string]$CompilerDirectory='D:\Qt\Qt6\Tools\mingw1310_64\bin',
    [string]$XPlaneRoot='G:\SteamLibrary\steamapps\common\X-Plane 12',
    [string]$Icao='ZUCK',
    [string]$BaselineRoot=''
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & (Join-Path $PSScriptRoot 'build-lua.ps1') -CompilerDirectory $CompilerDirectory
    $compiler=Join-Path $CompilerDirectory 'g++.exe'
    $common=@('-std=c++17','-O2','-DOANS_DEBUG=0','-static','-Isrc','-Ivendor/lua-5.4.9/src')
    & $compiler @common 'src/core.cpp' 'src/runway_ahead.cpp' 'src/a350_map.cpp' 'tools/map_reuse_probe.cpp' 'build/lua/liblua.a' '-o' 'build/map_reuse_probe.exe'
    if($LASTEXITCODE -ne 0){throw 'Reuse probe compilation failed'}
    $buildings=Join-Path $projectRoot ('cache/dsf_buildings_'+$Icao+'.json')
    if(!(Test-Path -LiteralPath $buildings)){$buildings=Join-Path $projectRoot ('cache/osm2_'+$Icao+'.json')}
    & 'build/map_reuse_probe.exe' 'assets' $XPlaneRoot $Icao $buildings '--require-reuse'
    if($LASTEXITCODE -ne 0){throw 'Reuse regression failed'}
    if($BaselineRoot) {
        $baseline=[IO.Path]::GetFullPath((Join-Path $projectRoot $BaselineRoot))
        $exe=Join-Path $baseline 'map_reuse_probe.exe'
        & $compiler "-I$baseline" @common 'src/core.cpp' 'src/runway_ahead.cpp' (Join-Path $baseline 'a350_map.cpp') 'tools/map_reuse_probe.cpp' 'build/lua/liblua.a' '-o' $exe
        if($LASTEXITCODE -ne 0){throw 'Baseline reuse probe compilation failed'}
        & $exe (Join-Path $baseline 'assets') $XPlaneRoot $Icao $buildings
        if($LASTEXITCODE -ne 0){throw 'Baseline reuse probe failed'}
    }
} finally {Pop-Location}

param(
    [string]$CompilerDirectory='D:\Qt\Qt6\Tools\mingw1310_64\bin',
    [string]$XPlaneRoot='G:\SteamLibrary\steamapps\common\X-Plane 12',
    [string]$Icao='ZUCK',
    [int]$Frames=300
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & (Join-Path $PSScriptRoot 'build-lua.ps1') -CompilerDirectory $CompilerDirectory
    $probe=Join-Path $projectRoot 'build/map_texture_probe.exe'
    & (Join-Path $CompilerDirectory 'g++.exe') '-std=c++17' '-O2' '-DOANS_DEBUG=0' '-DNOMINMAX' '-static' '-Wall' '-Wextra' '-Isrc' '-Ivendor/lua-5.4.9/src' 'src/core.cpp' 'src/runway_ahead.cpp' 'src/a350_map.cpp' 'tools/map_texture_probe.cpp' 'build/lua/liblua.a' '-lopengl32' '-lglu32' '-lgdi32' '-o' $probe
    if($LASTEXITCODE -ne 0){throw 'Texture probe compilation failed'}
    $buildings=Join-Path $projectRoot ('cache/dsf_buildings_'+$Icao+'.json')
    if(!(Test-Path -LiteralPath $buildings)){$buildings=Join-Path $projectRoot ('cache/osm2_'+$Icao+'.json')}
    & $probe 'assets' $XPlaneRoot $Icao $buildings $Frames
    if($LASTEXITCODE -ne 0){throw 'Texture regression/benchmark failed'}
} finally {Pop-Location}

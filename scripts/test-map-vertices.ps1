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
    $common=@('-std=c++17','-O2','-DOANS_DEBUG=0','-static','-Wall','-Wextra','-Isrc','-Ivendor/lua-5.4.9/src')
    & $compiler @common 'tests/lua_vertex_buffer_tests.cpp' 'build/lua/liblua.a' '-o' 'build/lua_vertex_buffer_tests.exe'
    if($LASTEXITCODE -ne 0){throw 'Vertex boundary tests compilation failed'}
    & 'build/lua_vertex_buffer_tests.exe'
    if($LASTEXITCODE -ne 0){throw 'Vertex boundary tests failed'}
    $buildings=Join-Path $projectRoot ('cache/dsf_buildings_'+$Icao+'.json')
    if(!(Test-Path -LiteralPath $buildings)){$buildings=Join-Path $projectRoot ('cache/osm2_'+$Icao+'.json')}
    $current=Join-Path $projectRoot 'build/map-vertices-current.bin'
    & $compiler @common 'src/core.cpp' 'src/runway_ahead.cpp' 'src/a350_map.cpp' 'tools/map_vertex_probe.cpp' 'build/lua/liblua.a' '-o' 'build/map_vertex_probe.exe'
    if($LASTEXITCODE -ne 0){throw 'Vertex probe compilation failed'}
    & 'build/map_vertex_probe.exe' 'assets' $XPlaneRoot $Icao $buildings $current
    if($LASTEXITCODE -ne 0){throw 'Vertex serialization failed'}
    if($BaselineRoot) {
        # Snapshot contains the prior a350_map.cpp/.hpp and its matching assets.
        $baseline=[IO.Path]::GetFullPath((Join-Path $projectRoot $BaselineRoot))
        $exe=Join-Path $baseline 'map_vertex_probe.exe'
        $reference=Join-Path $projectRoot 'build/map-vertices-before.bin'
        # Use the matching public header in every translation unit as well;
        # snapshots may return owned vectors where the current API borrows them.
        & $compiler "-I$baseline" @common 'src/core.cpp' 'src/runway_ahead.cpp' (Join-Path $baseline 'a350_map.cpp') 'tools/map_vertex_probe.cpp' 'build/lua/liblua.a' '-o' $exe
        if($LASTEXITCODE -ne 0){throw 'Baseline vertex probe compilation failed'}
        & $exe (Join-Path $baseline 'assets') $XPlaneRoot $Icao $buildings $reference
        if($LASTEXITCODE -ne 0){throw 'Baseline vertex serialization failed'}
        $hash=(Get-FileHash -LiteralPath $current -Algorithm SHA256).Hash
        if($hash -ne (Get-FileHash -LiteralPath $reference -Algorithm SHA256).Hash){throw 'Vertex/label output differs from baseline'}
        Write-Output "PASS byte-identical vertices, colours, UVs, switch bounds and labels; SHA256=$hash"
    }
} finally {Pop-Location}

param(
    [string]$CompilerDirectory='D:\Qt\Qt6\Tools\mingw1310_64\bin',
    [string]$XPlaneRoot='G:\SteamLibrary\steamapps\common\X-Plane 12',
    [string]$Icao='ZUCK'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    & (Join-Path $PSScriptRoot 'build-lua.ps1') -CompilerDirectory $CompilerDirectory
    $compiler=Join-Path $CompilerDirectory 'g++.exe'
    $common=@('-std=c++17','-O2','-static','-Wall','-Wextra','-DNOMINMAX','-Isrc')
    & $compiler @common '-Ivendor/lua-5.4.9/src' 'src/core.cpp' 'src/runway_ahead.cpp' 'src/a350_map.cpp' 'tools/a350_map_probe.cpp' 'build/lua/liblua.a' '-o' 'build/a350_map_probe.exe'
    if($LASTEXITCODE -ne 0){throw 'Original A350 engine probe compile failed'}
    & $compiler @common 'tools/render_a350_buffer.cpp' '-lopengl32' '-lglu32' '-lgdi32' '-o' 'build/render_a350_buffer.exe'
    if($LASTEXITCODE -ne 0){throw 'WGL map renderer compile failed'}
    $cache=Join-Path $projectRoot ('cache/osm2_'+$Icao+'.json')
    $output=Join-Path $projectRoot ('reports/runtime/a350-'+$Icao+'-buildings')
    # The default pose is the observed ZUCK stand. Other airports use runway ref.
    if($Icao -eq 'ZUCK') {
        & 'build/a350_map_probe.exe' 'assets' $XPlaneRoot $Icao ($output+'.bin') $cache '29.716054916' '106.651611328' '250.77468872' '.5' '3' '--matrix'
    } else {
        & 'build/a350_map_probe.exe' 'assets' $XPlaneRoot $Icao ($output+'.bin') $cache
    }
    if($LASTEXITCODE -ne 0){throw 'Original A350 airport load/render failed'}
    & 'build/render_a350_buffer.exe' ($output+'.bin') 'assets/font_b612_px2.bin' ($output+'.bmp')
    if($LASTEXITCODE -ne 0){throw 'Actual A350 triangle buffer WGL render failed'}
    & 'build/render_a350_buffer.exe' ($output+'-traffic.bin') 'assets/font_b612_px2.bin' ($output+'-traffic.bmp')
    if($LASTEXITCODE -ne 0){throw 'Offline TCAS symbol render failed'}
    & 'build/render_a350_buffer.exe' ($output+'-adsb-labels.bin') 'assets/font_b612_px2.bin' ($output+'-adsb-labels.bmp')
    if($LASTEXITCODE -ne 0){throw 'Offline ADS-B label render failed'}
    Write-Output ('Offline renders only, NOT X-Plane screenshots: '+$output+'.bmp and '+$output+'-traffic.bmp')
} finally {Pop-Location}

param(
    [string]$SdkRoot = 'E:\cxx_project\_lib\SDK',
    [string]$CompilerDirectory = 'D:\Qt\Qt6\Tools\mingw1310_64\bin',
    [ValidateSet('Release','Debug')][string]$BuildFlavor = 'Release',
    [string]$AircraftPath = 'G:\SteamLibrary\steamapps\common\X-Plane 12\Aircraft\ToLissA320_V1p3p2'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$testRoot = Join-Path $projectRoot ('build/mock-host'+$(if($BuildFlavor -eq 'Debug'){'-debug'}else{''}))
$package = Join-Path $testRoot 'ToLissOANS'
$fixture = Join-Path $testRoot 'sim\Global Scenery\Global Airports\Earth nav data'
New-Item -ItemType Directory -Force -Path $testRoot,(Join-Path $package '64'),$fixture | Out-Null
$cycleFixture=Join-Path $testRoot 'sim\Custom Data'
New-Item -ItemType Directory -Force -Path $cycleFixture | Out-Null
@'
AIRAC cycle    : 2701
Valid (from/to): 21/JAN/2027 - 18/FEB/2027
'@ | Set-Content -LiteralPath (Join-Path $cycleFixture 'cycle_info.txt') -Encoding ascii
$binaryPackage=if($BuildFlavor -eq 'Debug'){'dist/ToLissOANS-debug/64/win.xpl'}else{'dist/ToLissOANS/64/win.xpl'}
Copy-Item -LiteralPath (Join-Path $projectRoot $binaryPackage) -Destination (Join-Path $package '64\win.xpl') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config.ini') -Destination $package -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'config') -Destination $package -Recurse -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets') -Destination $package -Recurse -Force
# Exercise actual plugin background extraction with an on-disk binary DSF,
# while also providing cached OSM fallbacks so lifecycle tests need no network.
& (Join-Path $projectRoot 'build/dsf_buildings_tests.exe') '--fixture' (Join-Path $fixture '+30+120/+30+120.dsf')
if ($LASTEXITCODE -ne 0) { throw 'DSF fixture generation failed' }
$buildingCache=Join-Path $package 'cache'
New-Item -ItemType Directory -Force $buildingCache | Out-Null
foreach($airportCode in @('ZTEST','ZALT','ZNEAR','VABB','VAAA','VCCC')) {
    $geometry=@(@{lon=120.01;lat=30.001},@{lon=120.012;lat=30.001},@{lon=120.012;lat=30.003},@{lon=120.01;lat=30.003},@{lon=120.01;lat=30.001})
    @{elements=@(@{type='way';id=1;tags=@{aeroway='terminal'};geometry=$geometry})} | ConvertTo-Json -Depth 8 -Compress | Set-Content -LiteralPath (Join-Path $buildingCache ('osm2_'+$airportCode+'.json')) -Encoding utf8NoBOM
}
@'
I
1200 test fixture
1 0 0 0 LOCAL_ID Test Airport
1302 icao_code ZTEST
1302 iata_code TST
1302 city Test City
1302 datum_lat 30.002
1302 datum_lon 120.01
14 30.0015 120.005 100 0 Tower
100 45 1 0 0.25 1 1 0 09 30.0 120.0 0 0 0 0 0 0 27 30.0 120.035 0 0 0 0 0 0
1201 30.0 120.02 both 1
1201 30.001 120.02 both 2
1202 1 2 twoway taxiway A1
1300 30.001 120.001 90 gate jets A1
1301 C airline
1300 30.001 120.002 90 gate jets A2
1301 C airline
1300 30.001 120.003 90 gate jets B1
1301 C airline
1300 30.001 120.004 90 gate jets B2
1301 C airline
1300 30.001 120.005 90 gate jets C1
1301 C airline
1300 30.001 120.006 90 gate jets C2
1301 C airline
1 0 0 0 ALT_ID Alternate Test Airport
1302 icao_code ZALT
100 45 1 0 0.25 1 1 0 18 31.0 120.0 0 0 0 0 0 0 36 31.03 120.0 0 0 0 0 0 0
1 0 0 0 NEAR_ID Nearby Test Airport
1302 icao_code ZNEAR
100 45 1 0 0.25 1 1 0 18 30.2 120.0 0 0 0 0 0 0 36 30.23 120.0 0 0 0 0 0 0
1 0 0 0 VABB Mumbai Test Airport
1302 icao_code VABB
1302 iata_code BOM
1302 city Mumbai
100 45 1 0 0.25 1 1 0 09 19.0 72.0 0 0 0 0 0 0 27 19.0 72.035 0 0 0 0 0 0
1 0 0 0 VAAA A Test Airport
1302 icao_code VAAA
1302 iata_code AAA
100 45 1 0 0.25 1 1 0 09 19.1 72.0 0 0 0 0 0 0 27 19.1 72.035 0 0 0 0 0 0
1 0 0 0 VCCC C Test Airport
1302 icao_code VCCC
1302 iata_code CCC
100 45 1 0 0.25 1 1 0 09 19.2 72.0 0 0 0 0 0 0 27 19.2 72.035 0 0 0 0 0 0
99
'@ | Set-Content -LiteralPath (Join-Path $fixture 'apt.dat') -Encoding utf8NoBOM
$compiler = Join-Path $CompilerDirectory 'g++.exe'
Push-Location $projectRoot
try {
    & $compiler '-std=c++17' '-O2' '-static' '-shared' '-DXPLM=1' '-DIBM=1' '-DAPL=0' '-DLIN=0' '-DXPLM200' '-DXPLM210' '-DXPLM300' '-DXPLM301' '-DXPLM303' "-I$SdkRoot\CHeaders\XPLM" 'tests/mock_xplm.cpp' '-o' (Join-Path $testRoot 'XPLM_64.dll')
    if ($LASTEXITCODE -ne 0) { throw 'Mock SDK compilation failed' }
    & $compiler '-std=c++17' '-O2' '-static' '-Ivendor/xptools/Utils' 'tests/plugin_host.cpp' 'src/aircraft_profile_generator.cpp' (Join-Path $projectRoot 'build/dsf/libdsf.a') '-o' (Join-Path $testRoot 'plugin_host.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Native host compilation failed' }
    & (Join-Path $testRoot 'plugin_host.exe') (Join-Path $testRoot 'XPLM_64.dll') (Join-Path $package '64\win.xpl') (Join-Path $testRoot 'sim') (Join-Path $AircraftPath 'a320.acf') '--test-failure-cleanup'
    if ($LASTEXITCODE -ne 0) { throw 'Native host failure-cleanup regression failed' }
    $dsfCache=Join-Path $buildingCache 'dsf_buildings_ZTEST.json'
    if(Test-Path -LiteralPath $dsfCache){Remove-Item -LiteralPath $dsfCache}
    & (Join-Path $testRoot 'plugin_host.exe') (Join-Path $testRoot 'XPLM_64.dll') (Join-Path $package '64\win.xpl') (Join-Path $testRoot 'sim') (Join-Path $AircraftPath 'a320.acf')
    if ($LASTEXITCODE -ne 0) { throw 'Native plugin lifecycle tests failed' }
    $dsfTile=Join-Path $fixture '+30+120/+30+120.dsf'
    Remove-Item -LiteralPath $dsfTile
    Copy-Item -LiteralPath (Join-Path $projectRoot 'config.ini') -Destination $package -Force
    & (Join-Path $testRoot 'plugin_host.exe') (Join-Path $testRoot 'XPLM_64.dll') (Join-Path $package '64\win.xpl') (Join-Path $testRoot 'sim') (Join-Path $AircraftPath 'a320.acf') '--test-dsf-cache'
    if ($LASTEXITCODE -ne 0) { throw 'DSF cache must work with no source DSF present' }
    & (Join-Path $projectRoot 'build/dsf_buildings_tests.exe') '--fixture' $dsfTile
    if ($LASTEXITCODE -ne 0) { throw 'DSF fixture regeneration failed' }
    '{broken-json' | Set-Content -LiteralPath $dsfCache -Encoding utf8NoBOM
    Copy-Item -LiteralPath (Join-Path $projectRoot 'config.ini') -Destination $package -Force
    & (Join-Path $testRoot 'plugin_host.exe') (Join-Path $testRoot 'XPLM_64.dll') (Join-Path $package '64\win.xpl') (Join-Path $testRoot 'sim') (Join-Path $AircraftPath 'a320.acf') '--test-dsf-reextract'
    if ($LASTEXITCODE -ne 0) { throw 'Corrupt DSF cache must be rebuilt from scenery' }
    Remove-Item -LiteralPath $dsfTile
    '{"schema":"oans-dsf-buildings-v1","buildings":[]}' | Set-Content -LiteralPath $dsfCache -Encoding utf8NoBOM
    Copy-Item -LiteralPath (Join-Path $projectRoot 'config.ini') -Destination $package -Force
    & (Join-Path $testRoot 'plugin_host.exe') (Join-Path $testRoot 'XPLM_64.dll') (Join-Path $package '64\win.xpl') (Join-Path $testRoot 'sim') (Join-Path $AircraftPath 'a320.acf') '--test-osm-cache'
    if ($LASTEXITCODE -ne 0) { throw 'Empty DSF cache and missing scenery must fall back to cached OSM' }
} finally { Pop-Location }

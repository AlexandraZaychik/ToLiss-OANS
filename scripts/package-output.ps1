param(
    [string]$CompilerDirectory='D:\Qt\Qt6\Tools\mingw1310_64\bin',
    [ValidateSet('Both','Release','Debug')][string]$BuildFlavor='Both'
)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$cmake=Get-Content -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Raw
$version=[regex]::Match($cmake,'project\(ToLissOANS VERSION ([0-9.]+)').Groups[1].Value
if(!$version){throw 'Version missing from CMakeLists.txt'}
$outputRoot=Join-Path $projectRoot 'output'
$pluginOutput=Join-Path $outputRoot '00_ToLissOANS'
$debugBuilt=Join-Path $projectRoot 'dist/ToLissOANS-debug'
$devRoot=Join-Path $outputRoot 'code[develop purpose only]'
$devOutput=Join-Path $devRoot ('ToLissOANS-'+$version)
$built=Join-Path $projectRoot 'dist/ToLissOANS'
$notices=Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md'
if(!(Test-Path -LiteralPath $notices)){$notices=Join-Path $projectRoot 'THIRD_PARTY_NOTICES.txt'}
$flavors=if($BuildFlavor -eq 'Both'){@('release','debug')}else{@($BuildFlavor.ToLowerInvariant())}
foreach($binaryRoot in @($flavors | ForEach-Object {if($_ -eq 'debug'){$debugBuilt}else{$built}})) {
    if(!(Test-Path -LiteralPath (Join-Path $binaryRoot '64/win.xpl'))){throw 'Run scripts/build.ps1 -BuildFlavor Both first'}
}
$backup=Join-Path $projectRoot ('backups/output-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force -Path $devRoot,$devOutput | Out-Null
function Copy-Tree([string]$from,[string]$to) {
    New-Item -ItemType Directory -Force $to | Out-Null
    foreach($file in Get-ChildItem -LiteralPath $from -Recurse -File) {
        $relative=[IO.Path]::GetRelativePath($from,$file.FullName)
        if($file.Extension -in @('.md','.log','.pyc','.wav','.mp3','.dll','.exe','.xpl','.zip','.rar','.gz') -or $relative -match '(^|[\\/])(reports|__pycache__|lua-5\.1\.5)([\\/]|$)'){continue}
        $destination=Join-Path $to $relative
        New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($destination)) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
    }
}
foreach($name in @('src','tests','tools','scripts','vendor','assets','config','cache','A350_BTV_v2.14.3')) {
    Copy-Tree (Join-Path $projectRoot $name) (Join-Path $devOutput $name)
}
foreach($name in @('CMakeLists.txt','config.ini','LICENSE','.gitignore')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $name) -Destination $devOutput -Force
}
Copy-Item -LiteralPath $notices -Destination (Join-Path $devOutput 'THIRD_PARTY_NOTICES.txt') -Force
# Preserve provenance in a text file, keeping the requested output free of MD/reports.
$provenance=Join-Path $projectRoot 'vendor/xptools/UPSTREAM.md'
if(!(Test-Path -LiteralPath $provenance)){$provenance=Join-Path $projectRoot 'vendor/xptools/UPSTREAM.txt'}
Copy-Item -LiteralPath $provenance -Destination (Join-Path $devOutput 'vendor/xptools/UPSTREAM.txt') -Force
foreach($flavor in $flavors) {
    $folder=if($flavor -eq 'debug'){'00_ToLissOANS-debug'}else{'00_ToLissOANS'}
    $built=if($flavor -eq 'debug'){$debugBuilt}else{Join-Path $projectRoot 'dist/ToLissOANS'}
    $pluginOutput=Join-Path $outputRoot $folder
    $stamp=(Get-Content -LiteralPath (Join-Path $built 'build-flavor.txt') -Raw).Trim()
    if($stamp -ne $flavor){throw "Wrong build flavor: $built ($stamp)"}
    # Archive the old package before creating a clean tree; exclude stale logs.
    if(Test-Path -LiteralPath $pluginOutput) {
        $resolved=[IO.Path]::GetFullPath($pluginOutput)
        $destination=[IO.Path]::GetFullPath((Join-Path $backup $folder))
        if([IO.Path]::GetDirectoryName($resolved) -ne $outputRoot -or
           !$destination.StartsWith([IO.Path]::GetFullPath((Join-Path $projectRoot 'backups'))+[IO.Path]::DirectorySeparatorChar)) {throw 'Unexpected package archive path'}
        New-Item -ItemType Directory -Force $backup | Out-Null
        Move-Item -LiteralPath $resolved -Destination $destination
    }
    New-Item -ItemType Directory -Force $pluginOutput | Out-Null
foreach($name in @('assets','cache','config')) {Copy-Tree (Join-Path $built $name) (Join-Path $pluginOutput $name)}
New-Item -ItemType Directory -Force (Join-Path $pluginOutput '64') | Out-Null
Copy-Item -LiteralPath (Join-Path $built '64/win.xpl') -Destination (Join-Path $pluginOutput '64/win.xpl') -Force
Copy-Item -LiteralPath (Join-Path $built 'config.ini'),(Join-Path $built 'LICENSE') -Destination $pluginOutput -Force
Copy-Item -LiteralPath $notices -Destination (Join-Path $pluginOutput 'THIRD_PARTY_NOTICES.txt') -Force
    @"
ToLiss OANS $version - $flavor (Windows x86-64)
Install exactly ONE package in the aircraft plugins folder. Do not install release and debug together.
00_ToLissOANS: normal use. detail.log records configuration lookup, validation and generation only.
00_ToLissOANS-debug: troubleshooting only; slower and verbose, with debug symbols in 64/win.xpl.
Before switching, move the other package out of plugins. Preserve your config.ini and custom config JSON files.
Reproduce the problem with debug, then send detail.log, any detail.log.1/.2/.3 files, and X-Plane Log.txt.
Include X-Plane version, aircraft version, graphics driver and the exact action causing the crash.
Logs contain local file paths. They append across sessions, rotate at 32 MiB, and keep four files total.
Exception records are first-chance observations and can also come from the host or another plugin.
A process kill, stack exhaustion, or a failure before XPluginStart can prevent an exception record.
The last unmatched ENTER/TRACE line can still narrow the failing call. A debug package does not fix compatibility by itself.
"@ | Set-Content -LiteralPath (Join-Path $pluginOutput 'INSTALL.txt') -Encoding utf8NoBOM
$manifest=[ordered]@{
    version=$version
    build_flavor=$flavor
    detail_log=$(if($flavor -eq 'debug'){'config + full callback/SDK trace + exception addresses'}else{'config only'})
    map_renderer='Original A350 BTV 2.14.3 Lua functions, private static Lua 5.4.9'
    architecture='Windows x86-64'
    building_source_priority=@('cached DSF','runtime scenery DSF facade extraction','cached OSM','Overpass download')
    dsf_reader='Static DSFLib + LZMA SDK; raw and 7z-compressed DSFs'
    external_helper_required=$false
    test_host_in_package=$false
    xplane_started_by_verifier=$false
    real_aircraft_runtime_verified=$false
    files=[ordered]@{}
}
$objdump=Join-Path $CompilerDirectory 'objdump.exe'
$imports=& $objdump '-p' (Join-Path $pluginOutput '64/win.xpl')
if($LASTEXITCODE -ne 0){throw 'Plugin import inspection failed'}
$manifest.imports=@($imports | Select-String 'DLL Name:\s*(.+)' | ForEach-Object {$_.Matches[0].Groups[1].Value.Trim()})
$manifest.exports=@('XPluginDisable','XPluginEnable','XPluginReceiveMessage','XPluginStart','XPluginStop')
foreach($file in Get-ChildItem -LiteralPath $pluginOutput -Recurse -File | Sort-Object FullName) {
    $relative=[IO.Path]::GetRelativePath($pluginOutput,$file.FullName).Replace('\','/')
    if($relative -eq 'manifest.json'){continue}
    if($file.Extension -eq '.md' -or $relative -match '(^|/)reports/'){throw "Unexpected output document: $relative"}
    $manifest.files[$relative]=@{bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $pluginOutput 'manifest.json') -Encoding utf8NoBOM
$builtHash=(Get-FileHash -LiteralPath (Join-Path $built '64/win.xpl')).Hash
if($builtHash -ne (Get-FileHash -LiteralPath (Join-Path $pluginOutput '64/win.xpl')).Hash){throw 'Output binary hash mismatch'}
    Write-Output "Packaged $flavor : $pluginOutput SHA256=$builtHash"
}
# Archive only obsolete version directories that belong to this output root.
foreach($old in Get-ChildItem -LiteralPath $devRoot -Directory) {
    if($old.FullName -eq $devOutput -or $old.Name -notmatch '^ToLissOANS-\d+\.\d+\.\d+$'){continue}
    $resolved=[IO.Path]::GetFullPath($old.FullName)
    if([IO.Path]::GetDirectoryName($resolved) -ne $devRoot){throw 'Unexpected archive source'}
    $destination=[IO.Path]::GetFullPath((Join-Path $backup $old.Name))
    if(!$destination.StartsWith([IO.Path]::GetFullPath((Join-Path $projectRoot 'backups'))+[IO.Path]::DirectorySeparatorChar)){throw 'Unexpected archive destination'}
    New-Item -ItemType Directory -Force $backup | Out-Null
    Move-Item -LiteralPath $resolved -Destination $destination
}
Write-Output "Updated output version $version; binary SHA256 $builtHash"
Write-Output "Selected packages ($($flavors -join ', ')): $outputRoot"
Write-Output "Source: $devOutput"

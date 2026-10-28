param([string]$CompilerDirectory='D:\Qt\Qt6\Tools\mingw1310_64\bin')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path $root ('build/log-collector-test-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$xplane=Join-Path $fixture '模拟器 XPlane'
$plugin=Join-Path $xplane 'Aircraft/Test aircraft/plugins/00_ToLissOANS-debug'
New-Item -ItemType Directory -Force $plugin | Out-Null
& (Join-Path $root 'scripts/build-log-collector.ps1') -CompilerDirectory $CompilerDirectory -Destination $plugin
Set-Content -LiteralPath (Join-Path $xplane 'X-Plane.exe') -Value 'fixture marker only'
Set-Content -LiteralPath (Join-Path $xplane 'Log.txt') -Value 'X-Plane fixture log'
foreach($name in @('detail.log','detail.log.1','diagnostics.log','其他.log','log/sub/deep.txt','logs/nested.log')) {
    $target=Join-Path $plugin $name
    New-Item -ItemType Directory -Force (Split-Path -Parent $target) | Out-Null
    Set-Content -LiteralPath $target -Value (('Log fixture '+$name+"`n")*200)
}
Set-Content -LiteralPath (Join-Path $plugin 'config.ini') -Value 'must not collect'
$live=[IO.File]::Open((Join-Path $plugin 'detail.log'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
try {
    # Deliberately invoke from a different working directory.
    Push-Location $root
    try { & (Join-Path $plugin 'CollectLogs.exe') --no-pause; if($LASTEXITCODE -ne 0){throw 'Collector failed'} }
    finally { Pop-Location }
} finally { $live.Dispose() }
$archives=@(Get-ChildItem -LiteralPath $plugin -Filter '*.7z')
if($archives.Count -ne 1){throw 'Expected exactly one archive'}
$extract=Join-Path $fixture 'extracted'
& (Join-Path $plugin 'tools/7zip/7z.exe') x $archives[0].FullName "-o$extract" -y | Out-Null
if($LASTEXITCODE -ne 0){throw 'Archive extraction failed'}
$expected=@('plugin/detail.log','plugin/detail.log.1','plugin/diagnostics.log','plugin/其他.log','plugin/log/sub/deep.txt','plugin/logs/nested.log','XPlane/Log.txt')
$actual=@(Get-ChildItem -LiteralPath $extract -Recurse -File | ForEach-Object {[IO.Path]::GetRelativePath($extract,$_.FullName).Replace('\','/')})
if(@(Compare-Object $expected $actual).Count){throw "Unexpected archive contents: $actual"}
foreach($relative in $expected) {
    $original=if($relative.StartsWith('plugin/')){Join-Path $plugin $relative.Substring(7)}else{Join-Path $xplane 'Log.txt'}
    if((Get-FileHash -LiteralPath $original).Hash -ne (Get-FileHash -LiteralPath (Join-Path $extract $relative)).Hash){throw "Snapshot mismatch: $relative"}
}
# A missing simulator log must be a visible failure, without a misleading archive.
Remove-Item -LiteralPath (Join-Path $xplane 'Log.txt')
& (Join-Path $plugin 'CollectLogs.exe') --no-pause
if($LASTEXITCODE -eq 0){throw 'Missing X-Plane log was silently accepted'}
if(@(Get-ChildItem -LiteralPath $plugin -Filter '*.7z').Count -ne 1){throw 'Failed collection left an archive'}
Write-Output "PASS: UTF-8 paths, spaces, root logs, rotated logs, nested log folders, open log, contents/hashes, missing X-Plane log. Fixture: $fixture"

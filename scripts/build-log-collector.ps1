param(
    [string]$CompilerDirectory='D:\Qt\Qt6\Tools\mingw1310_64\bin',
    [string]$Destination
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if(!$Destination){$Destination=Join-Path $projectRoot 'dist/ToLissOANS-debug'}
$vendor=Join-Path $projectRoot 'vendor/7zip'
foreach($name in @('7z.exe','7z.dll','License.txt','SOURCE.txt')) {
    if(!(Test-Path -LiteralPath (Join-Path $vendor $name))){throw "Missing bundled 7-Zip file: $name"}
}
New-Item -ItemType Directory -Force $Destination | Out-Null
& (Join-Path $CompilerDirectory 'g++.exe') '-std=c++17' '-O2' '-Wall' '-Wextra' '-static' '-s' '-municode' (Join-Path $projectRoot 'tools/collect_logs.cpp') '-o' (Join-Path $Destination 'CollectLogs.exe')
if($LASTEXITCODE -ne 0){throw 'Log collector compilation failed'}
$tools=Join-Path $Destination 'tools/7zip'
New-Item -ItemType Directory -Force $tools | Out-Null
foreach($name in @('7z.exe','7z.dll','License.txt','SOURCE.txt')) {
    Copy-Item -LiteralPath (Join-Path $vendor $name) -Destination $tools -Force
}
Write-Output "Built log collector: $Destination/CollectLogs.exe"

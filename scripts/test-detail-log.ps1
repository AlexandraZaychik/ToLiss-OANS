param([string]$CompilerDirectory='D:\Qt\Qt6\Tools\mingw1310_64\bin')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$runRoot=Join-Path $projectRoot ('build/detail-log-tests-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$compiler=Join-Path $CompilerDirectory 'g++.exe'
New-Item -ItemType Directory -Force $runRoot | Out-Null
Push-Location $projectRoot
try {
    foreach($flavor in @('release','debug')) {
        $define=if($flavor -eq 'debug'){'-DOANS_DEBUG=1'}else{'-DOANS_DEBUG=0'}
        $binary=Join-Path $runRoot ($flavor+'-tests.exe')
        & $compiler '-std=c++17' '-Og' '-g' '-static' '-Isrc' $define 'tests/detail_log_tests.cpp' '-o' $binary
        if($LASTEXITCODE -ne 0){throw "Logger $flavor test compile failed"}
        & $binary (Join-Path $runRoot $flavor)
        if($LASTEXITCODE -ne 0){throw "Logger $flavor tests failed"}
    }
    $faultRoot=Join-Path $runRoot 'fault'
    & (Join-Path $runRoot 'debug-tests.exe') $faultRoot '--fault'
    if($LASTEXITCODE -ne -1073741819){throw "Expected synthetic unhandled access violation exit, got $LASTEXITCODE"}
    $fault=Get-Content -LiteralPath (Join-Path $faultRoot 'detail.log') -Raw
    foreach($marker in @('before synthetic unhandled access violation','code=0xc0000005','target=0x1234','registers RIP=','observer-stack','module=','offset=0x')) {
        if(!$fault.Contains($marker)){throw "Missing exception breadcrumb: $marker"}
    }
    Write-Output 'PASS synthetic unhandled exception leaves addresses, registers and stack; exception still terminates child process'
    Write-Output "Evidence: $runRoot"
    $global:LASTEXITCODE=0 # the deliberately crashing child was the expected result
} finally {Pop-Location}

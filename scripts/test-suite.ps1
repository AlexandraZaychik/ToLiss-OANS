param(
    [switch]$UnitOnly,
    [string]$SdkRoot = 'E:\cxx_project\_lib\SDK',
    [string]$CompilerDirectory = 'D:\Qt\Qt6\Tools\mingw1310_64\bin',
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build/pruned'
$resultsDir = Join-Path $buildDir 'results'
New-Item -ItemType Directory -Force $resultsDir | Out-Null
$env:PATH = $CompilerDirectory + ';' + $env:PATH
Push-Location $projectRoot
try {
    & cmake -S $projectRoot -B $buildDir -G 'MinGW Makefiles' "-DCMAKE_C_COMPILER=$CompilerDirectory/gcc.exe" "-DCMAKE_CXX_COMPILER=$CompilerDirectory/g++.exe" "-DCMAKE_MAKE_PROGRAM=$CompilerDirectory/mingw32-make.exe" "-DSDK_ROOT=$SdkRoot" -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw 'Test configuration failed' }
    $targets = @('oans_tests','runway_ahead_tests','fms_tests','aircraft_profile_tests','dsf_buildings_tests','ui_tests','route_layer_tests','lua_vertex_buffer_tests','gl_state_tests','detail_log_tests')
    if (!$UnitOnly) { $targets += 'ToLissOANS' }
    & cmake --build $buildDir --parallel 4 --target @targets
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & ctest --test-dir $buildDir --output-on-failure --verbose | Tee-Object -FilePath (Join-Path $resultsDir 'unit.log')
    if ($LASTEXITCODE -ne 0) { throw 'C++ suite failed' }
    if (!$UnitOnly) {
        & (Join-Path $PSScriptRoot 'test-native.ps1') -SdkRoot $SdkRoot -CompilerDirectory $CompilerDirectory | Tee-Object -FilePath (Join-Path $resultsDir 'native.log')
        if ($LASTEXITCODE -ne 0) { throw 'Native integration suite failed' }
        & (Join-Path $PSScriptRoot 'test-menu-arrange.ps1') -Python $Python -SdkRoot $SdkRoot -CompilerDirectory $CompilerDirectory | Tee-Object -FilePath (Join-Path $resultsDir 'menu.log')
        if ($LASTEXITCODE -ne 0) { throw 'Menu suite failed' }
        & (Join-Path $PSScriptRoot 'test-detail-log.ps1') -CompilerDirectory $CompilerDirectory | Tee-Object -FilePath (Join-Path $resultsDir 'logger.log')
        if ($LASTEXITCODE -ne 0) { throw 'Debug logger suite failed' }
        & $Python -X utf8 (Join-Path $PSScriptRoot 'audit-tests.py') --logs (Join-Path $resultsDir 'unit.log') (Join-Path $resultsDir 'native.log') (Join-Path $resultsDir 'menu.log') (Join-Path $resultsDir 'logger.log')
        if ($LASTEXITCODE -ne 0) { throw 'Scenario inventory or execution coverage mismatch' }
    }
} finally { Pop-Location }

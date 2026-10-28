param(
    [string]$SdkRoot = 'E:\cxx_project\_lib\SDK',
    [string]$CompilerDirectory = 'D:\Qt\Qt6\Tools\mingw1310_64\bin',
    [switch]$SkipTests,
    [ValidateSet("Both","Release","Debug")][string]$BuildFlavor = "Both"
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $CompilerDirectory 'g++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw "Compiler not found: $compiler" }
if (!(Test-Path -LiteralPath (Join-Path $SdkRoot 'CHeaders\XPLM\XPLMPlugin.h'))) { throw "X-Plane SDK not found: $SdkRoot" }
$buildDir = Join-Path $projectRoot 'build'
$packageDir = Join-Path $projectRoot 'dist\ToLissOANS'
New-Item -ItemType Directory -Force -Path $buildDir,(Join-Path $packageDir '64') | Out-Null
$common = @('-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-static','-Isrc')
Push-Location $projectRoot
try {
    & (Join-Path $PSScriptRoot 'build-lua.ps1') -CompilerDirectory $CompilerDirectory -DebugSymbols
    & (Join-Path $PSScriptRoot 'build-dsf.ps1') -CompilerDirectory $CompilerDirectory -DebugSymbols
    $dsfArgs = @('-DNOMINMAX','-DLIL=1','-Ivendor/xptools/DSF','-Ivendor/xptools/Utils','-Ivendor/xptools/lzma19/C','src/dsf_buildings.cpp',(Join-Path $buildDir 'dsf/libdsf.a'))
    & $compiler @common 'src/core.cpp' 'tests/dsf_buildings_tests.cpp' @dsfArgs '-o' (Join-Path $buildDir 'dsf_buildings_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Runtime DSF test compilation failed' }
    if (!$SkipTests) {
        & (Join-Path $buildDir 'dsf_buildings_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Runtime DSF tests failed' }
    }
    & $compiler @common 'src/core.cpp' 'tools/dsf_buildings_probe.cpp' @dsfArgs '-o' (Join-Path $buildDir 'dsf_buildings_probe.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Runtime DSF probe compilation failed' }
    $luaArgs = @('-Ivendor/lua-5.4.9/src','src/a350_map.cpp',(Join-Path $buildDir 'lua/liblua.a'))
    & $compiler @common '-Ivendor/lua-5.4.9/src' 'tests/lua_vertex_buffer_tests.cpp' (Join-Path $buildDir 'lua/liblua.a') '-o' (Join-Path $buildDir 'lua_vertex_buffer_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Lua vertex buffer test compilation failed' }
    if (!$SkipTests) {
        & (Join-Path $buildDir 'lua_vertex_buffer_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Lua vertex buffer tests failed' }
    }
    & $compiler @common '-Ivendor/xptools/Utils' 'tests/aircraft_profile_generator_tests.cpp' 'src/aircraft_profile_generator.cpp' (Join-Path $buildDir 'dsf/libdsf.a') '-o' (Join-Path $buildDir 'aircraft_profile_generator_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Aircraft profile generator test compilation failed' }
    & $compiler @common 'tests/aircraft_profile_tests.cpp' '-o' (Join-Path $buildDir 'aircraft_profile_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Aircraft profile test compilation failed' }
    if (!$SkipTests) {
        & (Join-Path $buildDir 'aircraft_profile_tests.exe') $projectRoot
        if ($LASTEXITCODE -ne 0) { throw 'Aircraft profile tests failed' }
    }
    & $compiler @common 'src/core.cpp' 'tests/core_tests.cpp' '-o' (Join-Path $buildDir 'oans_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Core test compilation failed' }
    if (!$SkipTests) {
        & (Join-Path $buildDir 'oans_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
    }
    & $compiler @common 'src/core.cpp' 'src/runway_ahead.cpp' 'tests/runway_ahead_tests.cpp' '-o' (Join-Path $buildDir 'runway_ahead_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Runway advisory test compilation failed' }
    if (!$SkipTests) {
        & (Join-Path $buildDir 'runway_ahead_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Runway advisory tests failed' }
    }
    & $compiler @common 'src/fms.cpp' 'tests/fms_tests.cpp' '-o' (Join-Path $buildDir 'fms_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'FMS test compilation failed' }
    if (!$SkipTests) {
        & (Join-Path $buildDir 'fms_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'FMS tests failed' }
    }
    & $compiler @common 'src/fms.cpp' 'tools/fms_probe.cpp' '-o' (Join-Path $buildDir 'fms_probe.exe')
    if ($LASTEXITCODE -ne 0) { throw 'FMS live probe compilation failed' }
    & $compiler @common 'src/core.cpp' 'src/nd_ui.cpp' 'tests/ui_tests.cpp' '-o' (Join-Path $buildDir 'ui_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'ND UI test compilation failed' }
    if (!$SkipTests) {
        & (Join-Path $buildDir 'ui_tests.exe') (Join-Path $projectRoot 'reports\ND_UI.svg')
        if ($LASTEXITCODE -ne 0) { throw 'ND UI tests failed' }
    }
    & $compiler @common '-DNOMINMAX' 'tests/gl_state_tests.cpp' '-lopengl32' '-lglu32' '-lgdi32' '-o' (Join-Path $buildDir 'gl_state_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'OpenGL regression compilation failed' }
    if (!$SkipTests) {
        & (Join-Path $buildDir 'gl_state_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Real OpenGL pixel/state regression failed' }
    }
    & $compiler @common 'src/core.cpp' 'tools/apt_inspect.cpp' '-o' (Join-Path $buildDir 'apt_inspect.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Airport inspector compilation failed' }
    $flavors = if($BuildFlavor -eq 'Both'){@('Release','Debug')}else{@($BuildFlavor)}
    foreach($flavor in $flavors) {
        $packageName = if($flavor -eq 'Debug'){'ToLissOANS-debug'}else{'ToLissOANS'}
        $packageDir = Join-Path $projectRoot ('dist/'+$packageName)
        New-Item -ItemType Directory -Force (Join-Path $packageDir '64') | Out-Null
        $flavorArgs = if($flavor -eq 'Debug'){@('-DOANS_DEBUG=1','-Og','-g','-fno-omit-frame-pointer')}else{@('-DOANS_DEBUG=0','-O2','-s')}
        & $compiler @common @flavorArgs '-shared' '-DIBM=1' '-DAPL=0' '-DLIN=0' '-DXPLM200' '-DXPLM210' '-DXPLM300' '-DXPLM301' '-DXPLM303' '-DNOMINMAX' "-I$SdkRoot\CHeaders\XPLM" 'src/core.cpp' 'src/runway_ahead.cpp' 'src/fms.cpp' 'src/fms_xplm.cpp' 'src/nd_ui.cpp' 'src/plugin.cpp' 'src/aircraft_profile_generator.cpp' @luaArgs @dsfArgs (Join-Path $SdkRoot 'Libraries\Win\XPLM_64.lib') '-lopengl32' '-lglu32' '-lgdi32' '-lwinhttp' '-o' (Join-Path $packageDir '64\win.xpl')
        if ($LASTEXITCODE -ne 0) { throw 'Native plugin compilation failed' }
        Copy-Item -LiteralPath (Join-Path $projectRoot 'config.ini') -Destination $packageDir -Force
        Copy-Item -LiteralPath (Join-Path $projectRoot 'config') -Destination $packageDir -Recurse -Force
        Copy-Item -LiteralPath (Join-Path $projectRoot 'assets') -Destination $packageDir -Recurse -Force
        if(Test-Path -LiteralPath (Join-Path $projectRoot 'cache')) {
            Copy-Item -LiteralPath (Join-Path $projectRoot 'cache') -Destination $packageDir -Recurse -Force
        }
        # Exported developer sources deliberately omit Markdown/reports. Those
        # optional documents must not prevent a clean build from that source tree.
        foreach ($file in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md','THIRD_PARTY_NOTICES.txt','接手说明.md')) {
            $document=Join-Path $projectRoot $file
            if(Test-Path -LiteralPath $document) { Copy-Item -LiteralPath $document -Destination $packageDir -Force }
        }
        $reportSource=Join-Path $projectRoot 'reports'
        if(Test-Path -LiteralPath $reportSource) {
            $reportDir=Join-Path $packageDir 'reports'
            New-Item -ItemType Directory -Force $reportDir | Out-Null
            Get-ChildItem -LiteralPath $reportSource -File -Filter '*.md' | ForEach-Object {
                Copy-Item -LiteralPath $_.FullName -Destination $reportDir -Force
            }
        }
        $flavor.ToLowerInvariant() | Set-Content -LiteralPath (Join-Path $packageDir 'build-flavor.txt') -Encoding ascii
        if($flavor -eq 'Debug') {
            & (Join-Path $PSScriptRoot 'build-log-collector.ps1') -CompilerDirectory $CompilerDirectory -Destination $packageDir
        }
        Write-Output "Built $flavor native plugin: $(Join-Path $packageDir '64\win.xpl')"
    }
} finally { Pop-Location }
